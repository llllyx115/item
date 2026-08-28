#ifndef __MYSPI_SYS_H__
#define __MYSPI_SYS_H__

#include "stm32f4xx.h"

/* Òý½Å¶¨Òå: PB12-CS  PB13-SCK  PB14-MISO  PB15-MOSI (SPI2) */
#define SPI_GPIO_PORT       GPIOB
#define SPI_GPIO_CLK        RCC_AHB1Periph_GPIOB

#define SPI_PIN_CS          GPIO_Pin_12
#define SPI_PIN_SCK         GPIO_Pin_13
#define SPI_PIN_MISO        GPIO_Pin_14
#define SPI_PIN_MOSI        GPIO_Pin_15

#define SPI_SRC_SCK         GPIO_PinSource13
#define SPI_SRC_MISO        GPIO_PinSource14
#define SPI_SRC_MOSI        GPIO_PinSource15

#define SPIx                SPI2
#define SPIx_CLK            RCC_APB1Periph_SPI2
#define SPI_AF              GPIO_AF_SPI2

void MySPI_Init(void);
void MySPI_Start(void);
void MySPI_Stop(void);
uint8_t MySPI_SwapByte(uint8_t ByteSend);


void MySPI_SendBuf(const uint8_t *buf, uint32_t len);
void MySPI_RecvBuf(uint8_t *buf, uint32_t len);

#endif



