#ifndef  _OLED_H_
#define  _OLED_H_
#include "stm32f4xx.h"                  // Device header

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

void OLED_Init(void);
uint8_t IIC_SendCommondToOled(uint8_t Commond);
uint8_t OLED_WritePage(uint8_t *data,uint8_t count);
void SetPos(uint8_t page,uint8_t x);
uint8_t OLED_DisplayALine(void);
void OLED_Clear(void);
void OLED_ALLPage(void);
void SetPos(uint8_t page,uint8_t x);




#endif