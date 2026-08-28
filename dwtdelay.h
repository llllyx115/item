#ifndef _DWTDELAY_H_
#define _DWTDELAY_H_

#include "stm32f4xx.h"                  // Device header


void DWT_Delay_Init(void);
void MyDWTDelay_us(uint32_t us);
void MyDWTDelay_ms(uint32_t ms);


#endif

