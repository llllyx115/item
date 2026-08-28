#include "stm32f4xx.h"                  // Device header
#include "myspi.h"
/*
    引脚分配:
    CS   ---- PB12 (输出)
    CLK  ---- PB13 (输出)
    MISO ---- PB14 (输入) 
    MOSI ---- PB15 (输出) 
*/
#define SPI_CS_H()			GPIO_SetBits(SPI_GPIO_PORT, SPI_PIN_CS)
#define SPI_CS_L()			GPIO_ResetBits(SPI_GPIO_PORT, SPI_PIN_CS)

#define SPI_SCK_H()			GPIO_SetBits(SPI_GPIO_PORT, SPI_PIN_SCK)
#define SPI_SCK_L()			GPIO_ResetBits(SPI_GPIO_PORT, SPI_PIN_SCK)

#define SPI_MOSI_H()		GPIO_SetBits(SPI_GPIO_PORT, SPI_PIN_MOSI)
#define SPI_MOSI_L()		GPIO_ResetBits(SPI_GPIO_PORT, SPI_PIN_MOSI)

#define SPI_MISO_Read()		GPIO_ReadInputDataBit(SPI_GPIO_PORT,SPI_PIN_MISO)


//void MySPI_Init(void)
//{
//	//CS CLK MOSI ------>推挽输出   MISO---上拉输入
//	RCC_AHB1PeriphClockCmd(SPI_GPIO_CLK, ENABLE);
//	
//	GPIO_InitTypeDef GPIO_InitStruct;
//	GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_OUT;
//    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
//    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;      
//    GPIO_InitStruct.GPIO_Pin   = SPI_PIN_CS | SPI_PIN_SCK | SPI_PIN_MOSI;
//    GPIO_Init(SPI_GPIO_PORT, &GPIO_InitStruct);
//	
//	//MISO
//	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;
//	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
//	GPIO_InitStruct.GPIO_Pin = SPI_PIN_MISO;
//	GPIO_Init(SPI_GPIO_PORT,&GPIO_InitStruct);
//	
//	//模式0	
//	SPI_SCK_L();
//	SPI_CS_H();
//}

//void MySPI_Start(void)
//{
//	SPI_CS_L();
//}

//void MySPI_Stop(void)
//{
//	SPI_CS_H();
//}

//uint8_t MySPI_SwapByte(uint8_t ByteSend)  //数据交换
//{
//	uint8_t data_read = 0;
//	
//	for(uint8_t i = 0; i < 8; i++)
//	{
//		//在时钟上升沿到来时需要准备好数据
//		if(ByteSend & 0x80){
//			SPI_MOSI_H();
//		}else{
//			SPI_MOSI_L();
//		}
//		ByteSend <<= 1;
//		
//		
//		//读
//		SPI_SCK_H();
//		
//		data_read <<= 1;
//		if(SPI_MISO_Read() == 1){
//			data_read |= 0x1;
//		}
//		
//		SPI_SCK_L();		
//	}		
////	return data_read;
//}







