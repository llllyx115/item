#ifndef _RTC_H_
#define _RTC_H_
#include <stdio.h>
#include "stm32f4xx.h"                  // Device header


void My_Calender_RtcInit(void);
void RTC_MyGetTime(uint8_t *hour,uint8_t *minute,uint8_t *second);
void RTC_MyGetDate(uint16_t *year,uint8_t *month,uint8_t *date,uint8_t *week);


#endif
