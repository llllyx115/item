#ifndef _IIC_H_
#define _IIC_H_
#include "stm32f4xx.h"                  // Device header

/* IIC 基本时序  */
#define SDA_HIGH()  GPIO_SetBits(GPIOB, GPIO_Pin_9)
#define SDA_LOW()   GPIO_ResetBits(GPIOB, GPIO_Pin_9) 
#define SCL_HIGH()  GPIO_SetBits(GPIOB, GPIO_Pin_8)
#define SCL_LOW()   GPIO_ResetBits(GPIOB, GPIO_Pin_8)

void My_IIC_GpioInit();
void SDA_OUT(void);
void SDA_IN(void);
uint8_t SDARead(void);
void IIC_Start(void);
void IIC_SendByte(uint8_t data);
uint8_t IIC_WaitAck(void);
void IIC_Stop(void);
void IIC_Ack(void);
void IIC_NAck(void);
uint8_t IIC_ReadByte(uint8_t ack);

uint8_t AT24C02_WriteByte(uint8_t add,uint8_t data);
uint8_t AT24C02_ReadByte(uint8_t add);

#endif
