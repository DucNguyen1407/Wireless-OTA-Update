# Wireless-OTA-Update

This project updates firmware remotely wia Wi-Fi (Over-The-Air).

## Feature
* Updates via HTTP protocol.
* Has Web UI for visualization.
* CRC checksum validation before flashing.

## Used components
* **Hardware:** ESP32-WROOM-32, STM32F103C8T6, jumper wires, LED.
* **Required tools:** STM32CubeMX, KeilC, Arduino framework supported IDE.

### Setup Instructions

To ensure the OTA process functions correctly, configure your STM32 in CubeMX as follows:

#### 1. System Core 
* **SYS**: Set Debug to **Serial Wire**.
* **GPIO**: Configure GPIOA pin 0 as output, pin 1 as input.

#### 2. Connectivity 
* **USART1/2**: 
    * **Mode**: Asynchronous.
    * **Baud Rate**: `115200` bps.
 
To ensure the OTA process functions correctly, don't forget to configure the Application firmware starting address to your liking (mine was 0x800C800) in KeilC -> Options for target -> IROM1.


## Operating Steps
1. Flash STM32 Bootloader: Flash bootloader.bin to address 0x08000000 using ST-Link.

2. Get ESP32 IP: Power on ESP32 and check Serial Monitor for its Local IP. Open this IP in your browser to see the Wireless OTA Update page.

3. Upload Firmware: Select firmware.bin on the website and click Upload. ESP32 will send the file to STM32 via UART.

4. LED Indicators: Update Success: LED stays ON for 2 seconds. Running Mode: LED blinks every 500ms (New firmware is active).
   


## Contribution
Leave a Star if you find this helpful!
