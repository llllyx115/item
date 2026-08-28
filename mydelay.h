#ifndef _MYDELAY_H
#define _MYDELAY_H
#include "stm32f4xx.h"                  // Device header

void MyDelay_Init(void);
void Systick_Init(void);
void MyDelay_us(uint32_t num);
void MyDelay_ms(uint32_t num);
void MyDelay_s(uint32_t num);

#endif


