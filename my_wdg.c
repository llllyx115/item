#include "stm32f4xx.h"                  // Device header
#include "my_wdg.h"
void My_IWDG_Init(void)
{
	//
	//1.解锁写保护
	IWDG_WriteAccessCmd(ENABLE);
	//2.设置预分频系数 32分频
	IWDG_SetPrescaler(IWDG_Prescaler_32);
	//3.设置重装载值
	IWDG_SetReload(1000);//函数运行消耗1s
	//4.喂狗
	IWDG_ReloadCounter();
	//5.启动IWDG
	IWDG_Enable();
}