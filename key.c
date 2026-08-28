#include "stm32f4xx.h"
#include "key.h"



// 按键状态变量
uint8_t key0_pressed_flag = 0;  // 按键按下标志

// 初始化按键 K0（PE4）
void MyKeyInit(void)
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
    
    GPIO_InitTypeDef S;
    S.GPIO_Mode = GPIO_Mode_IN;
    S.GPIO_Pin = GPIO_Pin_4;
    S.GPIO_PuPd = GPIO_PuPd_UP;  // 上拉，按下为低电平
    GPIO_Init(GPIOE, &S);
}

// 检测按键是否被按下（带消抖和释放检测）
unsigned int MyKeyIsPressed(void)
{
    static uint8_t last_state = 1;
    uint8_t current_state = GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_4);
    
    if(last_state == 1 && current_state == 0)  // 检测到下降沿（按下）
    {
        // 消抖延时
        for(int i = 0; i < 50; i++)
        {
            for(int j = 0; j < 100; j++);
        }
        
        if(GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_4) == 0)
        {
            last_state = current_state;
            
            // 等待按键释放
            while(GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_4) == 0);
            
            // 释放后延时消抖
            for(int i = 0; i < 50; i++)
            {
                for(int j = 0; j < 100; j++);
            }
            
            last_state = 1;
            return 1;  // 按键有效按下
        }
    }
    last_state = current_state;
    return 0;  // 无按键
}

