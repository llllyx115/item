#ifndef __ESP01_H__
#define __ESP01_H__

#include "stm32f4xx.h"                  // Device header

extern uint8_t s_esp01_ready;

void Esp01_Init(void);

uint8_t Esp01_IsReady(void);

void Usart2_SendString(uint8_t *str);

uint8_t ESP01_SendCmd(char *cmd,char *ack,uint16_t waitetime);

void Usart2_SendBuf(uint8_t *buf, uint16_t len);

uint8_t ESP01_WaitAck(char *ack, uint16_t waittime);

#endif
