#include "stm32f4xx.h"                  // Device header

/*
  配置按键中断
*/

void My_Key_Exit(void)
{
	RCC_AHB1PeriphClockCmd (RCC_AHB1Periph_GPIOE ,ENABLE );//打开时钟
	RCC_APB2PeriphClockCmd (RCC_APB2Periph_SYSCFG ,ENABLE );//打开SYSCFG时钟
	
	//第一步：配置GPIO：（PE4 -- KEY0）
	GPIO_InitTypeDef S;
	
	S.GPIO_Mode = GPIO_Mode_IN ;//输入模式
	S.GPIO_Pin = GPIO_Pin_4;//PE4
	S.GPIO_PuPd = GPIO_PuPd_UP ;//上拉模式
	
	GPIO_Init (GPIOE ,&S);
	//第二步：SYSCFG 
	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOE ,EXTI_PinSource4);//PE4 告诉EXTI去检测PE4
	
	//EXTI配置（时钟默认是打开的）
	
	EXTI_InitTypeDef S1;
	S1.EXTI_Line = EXTI_Line4 ;//PE4->通道4
	S1.EXTI_LineCmd = ENABLE ;
	S1.EXTI_Mode = EXTI_Mode_Interrupt ;//中断（电信号传给NVIC）/ 事件（电信号传给外设）
	S1.EXTI_Trigger = EXTI_Trigger_Falling ;//下降沿检测
	
	EXTI_Init(&S1);
	
	//NVIC配置
	NVIC_InitTypeDef S2;
	
	S2.NVIC_IRQChannel = EXTI4_IRQn ;
	S2.NVIC_IRQChannelCmd = ENABLE ;
	S2.NVIC_IRQChannelPreemptionPriority = 6;
	S2.NVIC_IRQChannelSubPriority = 1;
	
	NVIC_Init (&S2);
	
}

void My_Fir_Exit(void)
{
	RCC_AHB1PeriphClockCmd (RCC_AHB1Periph_GPIOA ,ENABLE );//打开时钟
	RCC_APB2PeriphClockCmd (RCC_APB2Periph_SYSCFG ,ENABLE );//打开SYSCFG时钟
	
	//第一步：配置GPIO：（PA2 -- Fir）
	GPIO_InitTypeDef S;
	
	S.GPIO_Mode = GPIO_Mode_IN ;//输入模式
	S.GPIO_Pin = GPIO_Pin_2;//PE2
	S.GPIO_PuPd = GPIO_PuPd_UP ;//上拉模式
	
	GPIO_Init (GPIOE ,&S);
	//第二步：SYSCFG 
	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA ,EXTI_PinSource2);//PA2 告诉EXTI去检测PA2
	
	//EXTI配置（时钟默认是打开的）
	
	EXTI_InitTypeDef S1;
	S1.EXTI_Line = EXTI_Line2 ;//PA2->通道2
	S1.EXTI_LineCmd = ENABLE ;
	S1.EXTI_Mode = EXTI_Mode_Interrupt ;//中断（电信号传给NVIC）/ 事件（电信号传给外设）
	S1.EXTI_Trigger = EXTI_Trigger_Falling ;//下降沿检测
	
	EXTI_Init(&S1);
	
	//NVIC配置
	NVIC_InitTypeDef S2;
	
	S2.NVIC_IRQChannel = EXTI2_IRQn ;
	S2.NVIC_IRQChannelCmd = ENABLE ;
	S2.NVIC_IRQChannelPreemptionPriority = 0;
	S2.NVIC_IRQChannelSubPriority = 1;
	
	NVIC_Init (&S2);
	
}


