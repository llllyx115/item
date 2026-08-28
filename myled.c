#include "stm32f4xx.h"                  // Device header

void MyLedInit(void)
{
	//第一步：开时钟
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE);
	
	//初始化GPIOA
	//2. 配置 GPIO 结构体
	GPIO_InitTypeDef S;
	S.GPIO_Mode = GPIO_Mode_OUT;
	S.GPIO_OType = GPIO_OType_PP;  //推挽输出
	S.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
	//S.GPIO_PuPd = ;
	//S.GPIO_Speed = ;
	
	GPIO_Init(GPIOA,&S);
			
}

void MyLedOpen(void)
{
	//GPIO_Write(GPIOA,0x5);
	//GPIO_WriteBit(GPIOA,GPIO_Pin_6,0);
	//GPIO_SetBits();  //1
	
	GPIO_ResetBits(GPIOA,GPIO_Pin_6|GPIO_Pin_7);  //0
	
}

void MyLedClose(void)
{
	GPIO_SetBits(GPIOA,GPIO_Pin_6|GPIO_Pin_7);
	
}


 	