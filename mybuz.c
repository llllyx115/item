#include "stm32f4xx.h"                  // Device header

// 初始化蜂鸣器（PA6 输出模式）
void MyBuzInit(void)
{
    // 开时钟
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);

    // 初始化 GPIOA_Pin6 为输出模式
    GPIO_InitTypeDef S;
    S.GPIO_Mode = GPIO_Mode_OUT;        // ⚠️ 改为输出模式（你之前写的是输入）
    S.GPIO_OType = GPIO_OType_PP;       // 推挽输出
    S.GPIO_Pin = GPIO_Pin_6;
    S.GPIO_PuPd = GPIO_PuPd_NOPULL;     // 无上下拉
    S.GPIO_Speed = GPIO_Speed_50MHz;    // 输出速度

    GPIO_Init(GPIOE, &S);
}

// 开启蜂鸣器（PA6 输出高电平）
void MyBuzOn(void)
{
    GPIO_SetBits(GPIOE, GPIO_Pin_6);    // 高电平 → 蜂鸣器响（取决于硬件连接）
}

// 关闭蜂鸣器（PA6 输出低电平）
void MyBuzOff(void)
{
    GPIO_ResetBits(GPIOE, GPIO_Pin_6);  // 低电平 → 蜂鸣器停
}

//// 翻转蜂鸣器状态（如果响就停，停就响）
//void MyBuzToggle(void)
//{
//    // 读取当前电平，翻转后输出
//    if (GPIO_ReadOutputDataBit(GPIOA, GPIO_Pin_6) == Bit_SET)
//        GPIO_ResetBits(GPIOA, GPIO_Pin_6);
//    else
//        GPIO_SetBits(GPIOA, GPIO_Pin_6);
//}



