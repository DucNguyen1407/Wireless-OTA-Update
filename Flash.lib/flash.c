#include "FLASH.h"

void flash_erase(uint32_t addr) {
	FLASH_EraseInitTypeDef pErase;
	pErase.NbPages = 1;
	pErase.PageAddress = addr;
	pErase.TypeErase = 0x00;
	uint32_t PageError;
	HAL_FLASHEx_Erase(&pErase, &PageError);
}

void flash_write(uint32_t addr, uint8_t *mData, uint16_t length) {
	uint8_t status = 0x00;
	if(length % 2 == 1) {
		status = 1;
		length--;
	}
	for(uint32_t i = 0; i < length; i += 2) {
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr + i, *(mData + i) | (uint16_t)(*(mData + i + 1) << 8));
	}
	if(status == 1) HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr + length, *(mData + length) | (uint16_t)(0xFF << 8));
}

void flash_lock(void) {
	HAL_FLASH_Lock();
}

void flash_unlock(void){
	HAL_FLASH_Unlock();
}
