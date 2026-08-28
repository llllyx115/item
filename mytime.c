#include "stm32f4xx.h"                  // Device header
#include "mydelay.h"
#include "stdio.h"
#include "mytime.h"

extern volatile BaseType_t g_srf_queue_ret; 
extern volatile uint8_t g_srf_queue_flag;
//extern volatile uint32_t g_int_rise;
//extern volatile uint32_t g_int_fall;

void MyTim6_Init(void)
{
	//开时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6,ENABLE);
	
	
	//NVIC
//	NVIC_InitTypeDef S1;
//	S1.NVIC_IRQChannel = TIM6_DAC_IRQn;
//    S1.NVIC_IRQChannelCmd = ENABLE;
//    S1.NVIC_IRQChannelPreemptionPriority = 0;
//    S1.NVIC_IRQChannelSubPriority = 0;
//    NVIC_Init(&S1);
//	
	//实基单元初始化
	//(基本定时器只需要配置两个参数：TIM_Prescaler )
	//TIM_Period 重装载
		
	TIM_TimeBaseInitTypeDef S;
	S.TIM_Period = 65536 - 1; //重装载
	S.TIM_Prescaler = 84 - 1; //预分频
	TIM_TimeBaseInit(TIM6,&S);
	
	//TIM_ITConfig(TIM6,TIM_IT_Update,ENABLE);
	
	TIM_Cmd(TIM6,DISABLE); //定时器开始工作	
}


/* 测距 
	vcc - 5V
	Trig -- PC1
	Echo -- PC2
*/
void Srf05_Init(void)
{
	//GPIOC 初始化
	//EXTI --- SYSCFG-APB2
	
	//开启时钟GPIOC
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC,ENABLE);
    //使能SYSCFG时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG,ENABLE);
	
	
	//PC1 -- PP
	//配置GPIOC_1;
    GPIO_InitTypeDef Struct;
    Struct.GPIO_Mode = GPIO_Mode_OUT;
    Struct.GPIO_OType = GPIO_OType_PP;
    Struct.GPIO_Pin = GPIO_Pin_1;
    Struct.GPIO_Speed = GPIO_High_Speed;
    GPIO_Init(GPIOC,&Struct);
    
    GPIO_ResetBits(GPIOC,GPIO_Pin_1);  //默认拉低总线
	
	//配置GPIOC_2;设置为下拉输入
    GPIO_InitTypeDef Struct1;
    Struct1.GPIO_Mode = GPIO_Mode_IN;
    Struct1.GPIO_PuPd = GPIO_PuPd_DOWN;
	  //Struct1.GPIO_PuPd = GPIO_PuPd_NOPULL;
    Struct1.GPIO_Pin = GPIO_Pin_2;
    GPIO_Init(GPIOC,&Struct1);
	
	//SYSCFG设置
	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOC,EXTI_PinSource2);
	
	//EXTI配置
	
	EXTI_InitTypeDef S2;
	S2.EXTI_Line = EXTI_Line2;
	S2.EXTI_LineCmd = ENABLE;
	S2.EXTI_Mode = EXTI_Mode_Interrupt;
	S2.EXTI_Trigger = EXTI_Trigger_Rising_Falling; //双边沿
	EXTI_Init(&S2);
	
	
	//NVIC 
	NVIC_InitTypeDef S1;
	S1.NVIC_IRQChannel = EXTI2_IRQn;
    S1.NVIC_IRQChannelCmd = ENABLE;
    S1.NVIC_IRQChannelPreemptionPriority = 7;
    S1.NVIC_IRQChannelSubPriority = 0;
    NVIC_Init(&S1);
}

extern volatile uint8_t g_srf05_wait_echo;

void Srf05_Start(void)
{
	if(g_srf05_wait_echo != 0)
    {
        //上一次还没结束，直接放弃本次触发，防止冲突
        return;
    }
	GPIO_SetBits(GPIOC,GPIO_Pin_1);
	MyDelay_us(10);
	GPIO_ResetBits(GPIOC,GPIO_Pin_1);
	g_srf05_wait_echo = 1; // ✅标记：现在正在等待ECHO回波
}
