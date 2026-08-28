#include "stm32f4xx.h"                  // Device header
#include "stdio.h"

void DWT_Delay_Init(void)
{
    //使能 DWT 外设 
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    // 清零并启动周期计数器 
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

void MyDWTDelay_us(uint32_t us)
{
    if (DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) {
        // 硬件延时
        uint32_t start = DWT->CYCCNT;
        uint32_t ticks = us * (SystemCoreClock / 1000000);
        while ((DWT->CYCCNT - start) < ticks);
    } else {
        // ★ 软延时回退（不依赖任何中断） ★
        volatile uint32_t count = us * 12;  // 72MHz 下约 12 次循环 = 1us
        while (count--);
    }
}

void MyDWTDelay_ms(uint32_t ms)
{
    while(ms--) MyDWTDelay_us(1000);
}
