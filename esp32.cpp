#include <FS.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <WebServer.h>

const char *ssid = SSID_GOES_HERE;
const char *password = PASSWORD_GOES_HERE;

WebServer server(80);
File uploadFile;

#define START_BYTE 0xAA
#define ACK 0x79
#define NACK 0x1F
#define BLOCK_SIZE 256

uint16_t simpleCRC(uint8_t *data, int len)
{
    uint16_t crc = 0;
    for (int i = 0; i < len; i++)
    {
        crc += data[i];
    }
    return crc;
}

bool waitForAck(uint32_t timeout = 1000)
{
    uint32_t start = millis();
    while (millis() - start < timeout)
    {
        if (Serial2.available())
        {
            uint8_t resp = Serial2.read();
            return (resp == ACK);
        }
    }
    return false;
}

void handleUpload()
{
    HTTPUpload &upload = server.upload();

    if (upload.status == UPLOAD_FILE_START)
    {
        uploadFile = SPIFFS.open("/firmware.bin", FILE_WRITE);
        if (!uploadFile)
        {
            Serial.println("Cant open file /firmware.bin");
            return;
        }
        Serial.println("Upload...");
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (uploadFile)
            uploadFile.write(upload.buf, upload.currentSize);
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        if (uploadFile)
        {
            uploadFile.close();
            Serial.println("Upload done.");
        }
    }
}

void sendFirmwareToSTM32()
{
    File file = SPIFFS.open("/firmware.bin", FILE_READ);
    if (!file)
    {
        Serial.println("Cant open /firmware.bin");
        return;
    }

    uint8_t data[BLOCK_SIZE];
    uint8_t packet[BLOCK_SIZE + 5];
    int blockCount = 0;

    while (file.available())
    {
        int len = file.read(data, BLOCK_SIZE);
        uint16_t crc = simpleCRC(data, len);

        packet[0] = START_BYTE;
        packet[1] = (len >> 8) & 0xFF;
        packet[2] = len & 0xFF;
        memcpy(&packet[3], data, len);
        packet[3 + len] = (crc >> 8) & 0xFF;
        packet[4 + len] = crc & 0xFF;

        Serial.printf("packet[0] = %02X packet[1] = %02X packet[2] = %02X crc = %d\n", packet[0], packet[1], packet[2], crc);

        int retry = 3;
        while (retry--)
        {
            Serial2.write(packet, len + 5);
            Serial.printf("Sent block %d, waiting for ACK...\n", blockCount);

            if (waitForAck())
            {
                Serial.printf("Block %d: OK\n", blockCount);
                break;
            }
            else
            {
                Serial.printf("Block %d: NACK or timeout. Retrying...\n", blockCount);
            }
        }

        if (retry < 0)
        {
            Serial.println("Too many failed attempts. Aborting.");
            file.close();
            return;
        }

        blockCount++;
        delay(10);
    }

    packet[0] = START_BYTE;
    packet[1] = 0x00;
    packet[2] = 0x00;
    Serial2.write(packet, 3);

    file.close();
    Serial.println("Finished sending firmware.bin to STM32.");
}

void setup()
{
    Serial.begin(9600);
    Serial2.begin(115200);

    SPIFFS.begin(true);

    WiFi.begin(ssid, password);
    Serial.print("WiFi connecting...");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nWiFi connected. IP:");
    Serial.println(WiFi.localIP());

    server.on("/", HTTP_GET, []() {
      server.send(200, "text/html", R"rawliteral(
    <!DOCTYPE html>
    <html lang="en">
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>STM32 OTA Dashboard</title>
        <style>
            :root { --primary: #2563eb; --bg: #f1f5f9; --text: #1e293b; }
            body { font-family: 'Inter', sans-serif; background: var(--bg); color: var(--text); display: flex; justify-content: center; align-items: center; min-height: 100vh; margin: 0; }
            .container { background: white; width: 90%; max-width: 450px; padding: 2rem; border-radius: 1rem; box-shadow: 0 20px 25px -5px rgba(0,0,0,0.1); }
            h2 { margin-top: 0; font-size: 1.5rem; text-align: center; color: var(--primary); }
            .upload-box { border: 2px dashed #cbd5e1; border-radius: 0.75rem; padding: 2rem; text-align: center; margin-bottom: 1.5rem; transition: 0.3s; cursor: pointer; }
            .upload-box:hover { border-color: var(--primary); background: #eff6ff; }
            .btn { width: 100%; padding: 0.75rem; border: none; border-radius: 0.5rem; font-weight: 600; cursor: pointer; transition: 0.2s; margin-bottom: 0.75rem; }
            .btn-primary { background: var(--primary); color: white; }
            .btn-outline { background: white; border: 2px solid var(--primary); color: var(--primary); }
            .btn:disabled { background: #94a3b8; border-color: #94a3b8; cursor: not-allowed; }
            .progress-wrapper { height: 12px; background: #e2e8f0; border-radius: 6px; margin: 1.5rem 0; overflow: hidden; display: none; }
            #progress-bar { height: 100%; width: 0%; background: var(--primary); transition: width 0.3s; }
            #log-area { background: #0f172a; color: #38bdf8; padding: 1rem; border-radius: 0.5rem; font-family: monospace; font-size: 0.8rem; height: 100px; overflow-y: auto; margin-top: 1rem; }
        </style>
    </head>
    <body>
        <div class="container">
            <h2>Wireless OTA Update</h2>
            <div class="upload-box" onclick="document.getElementById('file-input').click()">
                <p id="file-name">Click to select .bin firmware</p>
                <input type="file" id="file-input" hidden accept=".bin" onchange="updateName()">
            </div>

            <div class="progress-wrapper" id="prog-wrap">
                <div id="progress-bar"></div>
            </div>

            <button class="btn btn-primary" id="up-btn" onclick="uploadFirmware()">1. Upload to ESP32</button>
            <button class="btn btn-outline" id="send-btn" onclick="sendToSTM32()">2. Flash to STM32</button>

            <div id="log-area">System ready...<br></div>
        </div>

        <script>
            const log = (msg) => { const el = document.getElementById('log-area'); el.innerHTML += `> ${msg}<br>`; el.scrollTop = el.scrollHeight; };
            
            function updateName() {
                const file = document.getElementById('file-input').files[0];
                document.getElementById('file-name').innerText = file ? file.name : "Select file";
            }

            function uploadFirmware() {
                const fileInput = document.getElementById('file-input');
                if (!fileInput.files[0]) return alert("Please select a file first!");

                const formData = new FormData();
                formData.append("file", fileInput.files[0]);

                const xhr = new XMLHttpRequest();
                document.getElementById('prog-wrap').style.display = 'block';
                document.getElementById('up-btn').disabled = true;

                xhr.upload.addEventListener("progress", (e) => {
                    const percent = (e.loaded / e.total) * 100;
                    document.getElementById('progress-bar').style.width = percent + "%";
                    log(`Uploading: ${Math.round(percent)}%`);
                });

                xhr.onreadystatechange = function() {
                    if (xhr.readyState === 4 && xhr.status === 200) {
                        log("Upload to ESP32 complete!");
                        document.getElementById('up-btn').disabled = false;
                    }
                };
                xhr.open("POST", "/upload", true);
                xhr.send(formData);
            }

            function sendToSTM32() {
                log("Starting Flash to STM32...");
                document.getElementById('send-btn').disabled = true;
                
                fetch('/send')
                    .then(res => res.text())
                    .then(data => {
                        log(data);
                        document.getElementById('send-btn').disabled = false;
                    })
                    .catch(err => log("Error: " + err));
            }
        </script>
    </body>
    </html>
      )rawliteral");
    });

    server.on("/upload", HTTP_POST, []()
              { server.send(200, "text/html", R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
      <title>Upload Firmware</title>
      <style>
        body {
          background-color: #f0f0f0;
          font-family: Arial, sans-serif;
          text-align: center;
          padding-top: 50px;
        }
        form {
          background: #fff;
          padding: 20px;
          border-radius: 10px;
          display: inline-block;
          box-shadow: 0 0 10px rgba(0,0,0,0.1);
        }
        input[type="file"] {
          margin-bottom: 15px;
        }
        input[type="submit"], a.button {
          background-color: #4CAF50;
          color: white;
          padding: 10px 20px;
          text-decoration: none;
          border: none;
          border-radius: 5px;
          cursor: pointer;
        }
        a.button {
          display: inline-block;
          margin-top: 15px;
        }
      </style>
    </head>
    <body>
      <form method="POST" action="/upload" enctype="multipart/form-data">
        <h2>OTA - STM32 - ESP32</h2>
        <a href="/send" class="button">Send to STM32</a>
      </form>
    </body>
    </html>
  )rawliteral"); }, handleUpload);

    server.on("/send", HTTP_GET, []()
              {
    server.send(200, "text/plain", "Sending firmware.bin to STM32...");
    sendFirmwareToSTM32(); });

    server.begin();
}

void loop()
{
    server.handleClient();
}