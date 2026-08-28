#ifndef __MYSPI_H__
#define __MYSPI_H__
#include "stm32f4xx.h"                  // Device header


//引脚定义
#define SPI_GPIO_PORT  	GPIOB
#define SPI_GPIO_CLK	RCC_AHB1Periph_GPIOB

#define SPI_PIN_CS		GPIO_Pin_12
#define SPI_PIN_SCK		GPIO_Pin_13
#define SPI_PIN_MISO	GPIO_Pin_14
#define SPI_PIN_MOSI	GPIO_Pin_15

//函数
void MySPI_Init(void);
void MySPI_Start(void);
void MySPI_Stop(void);
uint8_t MySPI_SwapByte(uint8_t ByteSend);  //数据交换


#endif

