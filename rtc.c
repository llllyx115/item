#include "stm32f4xx.h"                  // Device header
#include <stdio.h>

#define Flag_Change 0  //用防止每次复位重新设置RTC （宏开关）


void My_Calender_RtcInit(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR,ENABLE);
	
	//解除写保护
	PWR_BackupAccessCmd(ENABLE);
	
	#if(Flag_Change == 1)
RTC_WriteBackupRegister(RTC_BKP_DR0,0xbbcc);
	#endif
	
	//第一次读数的时候，一定不是0xaacc，所以满足if，进入，下电后，
	//函数运行结束，值设定为0xaacc，不进入函数，保留上次的时间。
	//如果想修改上电时的时间，就将Flag值改为1即可。
	
	if(RTC_ReadBackupRegister(RTC_BKP_DR0) != 0xaacc)//只要不等于Oxaacc
	{
	//1,使能LSE
	RCC_LSEConfig(RCC_LSE_ON);
	//rcc稳定产生时钟原需要一定的时间
	//2.等待LSE稳定
	while(RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET);
	//3.选择LSE作为RTC的时钟源 将LSE时钟源接入到RTC中
	RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
	//4.使能RTC时钟 让时钟信号进来
	RCC_RTCCLKCmd(ENABLE);
	//5.RTC初始化 预分频器
	RTC_InitTypeDef S;
	S.RTC_AsynchPrediv = 128 - 1;//异步
	S.RTC_HourFormat = RTC_HourFormat_24;
	S.RTC_SynchPrediv = 256-1;//同步
	if(RTC_Init(&S) != ERROR)
	{
		printf("[RTC] rtc init ok\n");
	}
	
	//初始化时间和日期 12:00:00
	RTC_TimeTypeDef S1;
	S1.RTC_H12 = RTC_H12_PM;
	S1.RTC_Hours = 11;
	S1.RTC_Minutes = 50;
	S1.RTC_Seconds = 0;
	RTC_SetTime(RTC_Format_BIN,&S1);
	
	//初始化日期：2026-8-1 星期六Saturday
	RTC_DateTypeDef S2;
	S2.RTC_Year = 26;
	S2.RTC_Month = 8;
	S2.RTC_Date = 3;
	S2.RTC_WeekDay = RTC_Weekday_Monday;
	RTC_SetDate(RTC_Format_BIN,&S2);
	
	RTC_WriteBackupRegister(RTC_BKP_DR0,0xaacc);//保留上次函数结束的时间
	
	PWR_BackupAccessCmd(DISABLE);
 }
}

void RTC_MyGetTime(uint8_t *hour,uint8_t *minute,uint8_t *second)
{
	//获取时间的函数（库函数）
	RTC_TimeTypeDef S;
	RTC_GetTime(RTC_Format_BIN,&S);
	
	*hour = S.RTC_Hours;
	*minute = S.RTC_Minutes;
	*second = S.RTC_Seconds;
	
}

void RTC_MyGetDate(uint16_t *year,uint8_t *month,uint8_t *date,uint8_t *week)
{
	RTC_DateTypeDef S;
	RTC_GetDate(RTC_Format_BIN,&S);
	
	*year = S.RTC_Year+2000;//20xx年
	*month = S.RTC_Month;
	*date = S.RTC_Date;
	*week = S.RTC_WeekDay;
}