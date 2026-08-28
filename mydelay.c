#include "stm32f4xx.h"                  // Device header

void Systick_Init(void)
{
	//时钟源的选择、要不要开中断、清除当前的计数值
	SysTick->CTRL &= ~(1 << 0);
	SysTick->CTRL &= ~(1 << 2);
	SysTick->CTRL &= ~(1 << 1);
	SysTick->VAL = 0x0;//COUNTFLAG清零
}
	
void MyDelay_Init(void)
{
  /* 开启DWT内核计数器，Cortex‑M4 */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void MyDelay_us(uint32_t nus)
{
    uint32_t t0 = DWT->CYCCNT;
    uint32_t ticks = nus * (SystemCoreClock / 1000000UL);
    while((DWT->CYCCNT - t0) < ticks);
}

//void MyDelay_us(uint32_t num)
//{
//	while(num--)
//	{
//	SysTick->LOAD = 21-1; //1us
//	SysTick->CTRL |= (1 << 0);//使能定时器
//	
//	while(!(SysTick->CTRL & (1 << 16)));//当未减到0时 条件成立 一直在while循环里 阻塞等待
//	//关闭定时器
//	SysTick->CTRL &= ~(1 << 0);
//	//清除VAL
//	SysTick->VAL = 0x0;
//	}
//}
	
void MyDelay_ms(uint32_t num)
{
	MyDelay_us(num * 1000);
}
	
void MyDelay_s(uint32_t num)
{
	MyDelay_ms(num * 1000);
}
	
