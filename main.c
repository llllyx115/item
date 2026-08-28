#define LOG_TAG "main"

#include "stm32f4xx.h"                  // Device header
#include "myled.h"
#include "myfir.h"
#include "mybuz.h"
#include "key.h"
#include "myusart1.h"
#include "stdio.h"
#include "string.h"
#include "my_exti.h"
#include "mydelay.h"
#include "dwtdelay.h"
#include "dht11.h"
#include "mytime.h"
#include "my_wdg.h"
#include "rtc.h"
#include "IIc.h"
//#include "myoled.h"
#include "ssd1306.h"
#include "dma.h"
#include "w25q64.h"
#include "ff.h"
#include "FreeRTOS.h"
#include "task.h"
#include "app.h"
#include "queue.h"
#include "elog.h"


extern volatile uint8_t  USART1_Rx_Flag;
uint8_t oled_arr[8][128] = {0};

int main(void)
{
	//中断优先级分组
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4); 
	
	//USART1初始化	
	MyUsart1Init();
	MyDelay_Init();
	MyTim6_Init();
	DWT_Delay_Init();
	W25Q64_Init();

	
	/* 日志初始化 */
	elog_init();
	
	//设置日志格式
	 /* 出错时信息给全，方便定位 */
    elog_set_fmt(ELOG_LVL_ASSERT, ELOG_FMT_ALL & ~ELOG_FMT_P_INFO);
    elog_set_fmt(ELOG_LVL_ERROR,  ELOG_FMT_ALL & ~ELOG_FMT_P_INFO);

    /* 警告要知道来源，不要行号 */
    elog_set_fmt(ELOG_LVL_WARN,
                 ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME | ELOG_FMT_T_INFO);

    /* 业务流水，简洁 */
    elog_set_fmt(ELOG_LVL_INFO,
                 ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME);

    /* 调试信息，带任务名和行号 */
    elog_set_fmt(ELOG_LVL_DEBUG,
                 ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME |
                 ELOG_FMT_T_INFO | ELOG_FMT_LINE);

    elog_set_fmt(ELOG_LVL_VERBOSE,
                 ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME |
                 ELOG_FMT_T_INFO | ELOG_FMT_LINE);
	/* 开启日志调试 */
	elog_start();
	
	
	BaseType_t xReturn;
	xReturn = xTaskCreate( 
		(TaskFunction_t) Start_Task, //任务函数
						"Start_Task", //任务名称--内核
		(configSTACK_DEPTH_TYPE ) 2048, //栈的大小
		(void *)  NULL, //任务参数
		(UBaseType_t) 1,  //优先级
		(TaskHandle_t *)  &StartTask_TaskHandle );
	
		if(xReturn == pdPASS){
			
			log_d(" start task create OK");
		}
		
		//开启任务调度器，进入RTOS的世界，永不返回
		vTaskStartScheduler();
		log_e("heap no enough");
}

