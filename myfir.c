#include "stm32f4xx.h"                  // Device header
#include "myfir.h"

/*
D0---PA2

输入---上拉
*/

void MyFirInit(void)
{
	//开时钟
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE);
	
	//初始化GPIOA
	GPIO_InitTypeDef S;
	S.GPIO_Mode = GPIO_Mode_IN;
	S.GPIO_Pin = GPIO_Pin_2;
	S.GPIO_PuPd = GPIO_PuPd_UP;//感应到火 是低电平 所以初始化为高电平 用上拉电阻
	
	GPIO_Init(GPIOA,&S);
	
}

void MyFirOpen(void)
{
	
	GPIO_ResetBits(GPIOA,GPIO_Pin_2);  //0
	
}

void MyFirClose(void)
{
	GPIO_SetBits(GPIOA,GPIO_Pin_2);
}




