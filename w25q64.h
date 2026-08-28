//#ifndef __W25Q64_H__
//#define __W25Q64_H__
//#include "stm32f4xx.h"                  // Device header


//void W25Q64_Init(void);

////忙等待
//uint8_t W25Q64_BusyWait(void);
////写使能
//void W25Q64_WriteEnable(void);
////读设备ID
//uint32_t W25Q64_ReadID(void);

////扇区擦除
//uint8_t W25Q64_SectorErase(uint32_t address);
//void W25Q64_PageWrite(uint32_t address,uint8_t *data,uint16_t count);
//void W25Q64_ReadDate(uint32_t address,uint8_t *arr,uint32_t count);
//void W25Q64_Write_janwenl(uint8_t *pBuffer, uint32_t WriteAddr, uint32_t NumByteToWrite);
//uint8_t W25Q64_ChipErase(void);


//void MyFile_ReadTest(void);
//void MyFile_WriteTest(void);
//uint8_t FatFs_Setup(void);

//#endif


#ifndef __W25Q64_H__
#define __W25Q64_H__
#include "stm32f4xx.h"

#define W25Q64_SECTOR_SIZE      4096
#define W25Q64_PAGE_SIZE        256
#define W25Q64_SECTOR_COUNT     2048        /* 8MB / 4096 */

void     W25Q64_Init(void);
uint8_t  W25Q64_BusyWait(void);
void     W25Q64_WriteEnable(void);
uint32_t W25Q64_ReadID(void);

uint8_t  W25Q64_SectorErase(uint32_t address);
uint8_t  W25Q64_ChipErase(void);
void     W25Q64_PageWrite(uint32_t address, uint8_t *data, uint16_t count);
void     W25Q64_ReadDate(uint32_t address, uint8_t *arr, uint32_t count);
void     W25Q64_WriteNoCheck(const uint8_t *buffer, uint32_t WriteAddr, uint32_t NumByteToWrite);
void     W25Q64_Write_janwenl(uint8_t *pBuffer, uint32_t WriteAddr, uint32_t NumByteToWrite);
uint8_t FatFs_Setup(void);
void MyFile_WriteTest(void);
void MyFile_ReadTest(void);


#endif