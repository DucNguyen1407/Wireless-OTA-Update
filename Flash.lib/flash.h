#ifndef __FLASH_H
#define __FLASH_H
#include <stm32f1xx_hal.h>

void flash_erase(uint32_t addr);

void flash_write(uint32_t addr, uint8_t *mData, uint16_t length);

void flash_lock(void);

void flash_unlock(void);

#endif

