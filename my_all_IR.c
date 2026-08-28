#include "stm32f4xx.h"                  // Device header
#include "stdio.h"
#include "myusart1.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "mytime.h"
#include "buffer.h"

/********************* USART2 **************************/
uint8_t s_esp01_rx_mem[ESP01_RX_BUF_SIZE];
BufferTypeDef g_esp01_rx;    /* 环形缓冲区 */
SemaphoreHandle_t g_esp01_rx_sem = NULL; /* 一帧接收完成通知 */

void USART2_IRQHandler(void)
{
	BaseType_t xWoken = pdFALSE;

	/* 收到一个字节 */
    if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    {
        uint8_t d = (uint8_t)USART_ReceiveData(USART2);
        Buffer_Push(&g_esp01_rx, d);
    }

	/* 触发空闲中断 */
	if(USART_GetITStatus(USART2, USART_IT_IDLE) != RESET)
	{
		//IDLE清除 先读SR 再读DR
		(void)USART2->SR;
		(void)USART2->DR;

		if(g_esp01_rx_sem != NULL){
			xSemaphoreGiveFromISR(g_esp01_rx_sem,&xWoken);
		}
	}
	portYIELD_FROM_ISR(xWoken);
}

// 0=空闲，可以触发测量；1=正在等待ECHO回波中
volatile uint8_t g_srf05_wait_echo = 0;
volatile uint32_t g_int_rise = 0;
volatile uint32_t g_int_fall = 0;

/* EXTI2 ----- SRF05测距 */
void EXTI2_IRQHandler(void)
{
    BaseType_t xWoken = pdFALSE;
    BaseType_t queue_ret;

    if(EXTI_GetITStatus(EXTI_Line2) == SET)
    {
        if(g_srf05_wait_echo == 1)  /* 只有正在等待回波，才处理中断 */
        {
            if(GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_2) == 1)
            {
                /* 上升沿：开始计时 */
                g_int_rise++;
                TIM_Cmd(TIM6, DISABLE);
                TIM_SetCounter(TIM6, 0);
                TIM_Cmd(TIM6, ENABLE);
            }
            else
            {
                /* 下降沿：回波结束 */
                g_int_fall++;
                TIM_Cmd(TIM6, DISABLE);

                SRF05_Data_t d;
                d.time_us = TIM_GetCounter(TIM6);
                d.tick    = xTaskGetTickCountFromISR();

                /* 【修复】只发送一次！原代码发送了两次，导致队列里有重复数据 */
                queue_ret = xQueueSendFromISR(SRF05_Queue, &d, &xWoken);
                g_srf_queue_ret  = queue_ret;
                g_srf_queue_flag = 1;

                g_srf05_wait_echo = 0; /* 测量结束，释放状态 */
            }
        }
        EXTI_ClearITPendingBit(EXTI_Line2);
    }
    portYIELD_FROM_ISR(xWoken);
}

/********************* USART1 **************************/
#define USART1_RX_BUF_SIZE  256
#define USART1_FRAME_SIZE   128

volatile uint8_t  USART1_Rx_Buf[USART1_RX_BUF_SIZE];
volatile uint16_t USART1_Rx_Write_Idx = 0;
volatile uint16_t USART1_Rx_Read_Idx = 0;
volatile uint16_t USART1_Rx_DataLen = 0;
volatile uint8_t  USART1_Rx_Flag = 0;
volatile uint8_t  USART1_Rx_Overflow = 0;

uint16_t Ring_Datalen(uint16_t write_idx, uint16_t read_idx)
{
	if (write_idx >= read_idx) {
        return (uint16_t)(write_idx - read_idx);
    }
    return (uint16_t)(USART1_RX_BUF_SIZE - read_idx + write_idx);
}

uint16_t Ring_Next(uint16_t idx)
{
	return (uint16_t)((idx + 1) % USART1_RX_BUF_SIZE);
}

void USART1_ClearItFlag(USART_TypeDef * USARTx)
{
	uint32_t tmp;
	tmp = USARTx->SR;
	tmp = USARTx->DR;
	(void)tmp;
}

uint16_t USART1_ReadFrame(uint8_t *buf, uint16_t data_len)
{
	uint16_t read_len;

	if(USART1_Rx_Flag == 0 || USART1_Rx_DataLen == 0){
		return 0;
	}
	read_len = (USART1_Rx_DataLen < data_len)?USART1_Rx_DataLen:data_len;

	for( uint16_t i = 0; i < read_len; i++)
	{
		buf[i] = USART1_Rx_Buf[USART1_Rx_Read_Idx];
		USART1_Rx_Read_Idx = Ring_Next(USART1_Rx_Read_Idx);
	}
	USART1_Rx_Buf[read_len] = '\0';

	USART1_Rx_Flag = 0;
	USART1_Rx_DataLen = 0;

	return read_len;
}

void USART1_IRQHandler(void)
{
	uint8_t recv_data;
	uint16_t next_write_idx;

	if(USART_GetITStatus(USART1,USART_IT_RXNE) == SET)
	{
		recv_data = (uint8_t)USART_ReceiveData(USART1);
		next_write_idx = Ring_Next(USART1_Rx_Write_Idx);
		if(next_write_idx != USART1_Rx_Read_Idx)
		{
			USART1_Rx_Buf[USART1_Rx_Write_Idx] = recv_data;
			USART1_Rx_Write_Idx = next_write_idx;
		}else{
			USART1_Rx_Overflow = 1;
		}
	}
	if(USART_GetITStatus(USART1,USART_IT_IDLE) == SET)
	{
		if(USART1_Rx_Flag == 0){
			USART1_Rx_DataLen = Ring_Datalen(USART1_Rx_Write_Idx,
													USART1_Rx_Read_Idx);
			if(USART1_Rx_DataLen > 0)
			{
				USART1_Rx_Flag = 1;
			}
		}
		USART1_ClearItFlag(USART1);
	}
}
