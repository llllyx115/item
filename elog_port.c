/*
 * This file is part of the EasyLogger Library.
 *
 * Copyright (c) 2015, Armink, <armink.ztl@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * 'Software'), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Function: Portable interface for each platform.
 * Created on: 2015-04-28
 */
 
#include <elog.h>

#include "stm32f4xx.h"                  // Device header
#include <elog.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "elog_port.h"

SemaphoreHandle_t s_elog_mutex = NULL;  //互斥锁
/**
 * EasyLogger port initialize
 *
 * @return result
 */
ElogErrCode elog_port_init(void) {
    ElogErrCode result = ELOG_NO_ERR;

    /* add your code here */
    s_elog_mutex = NULL;  //明确初始化为NULL
    return result;
}
/**
 * EasyLogger port deinitialize
 *
 */
void elog_port_deinit(void) {

    /* add your code here */
	if(s_elog_mutex != NULL){
		vSemaphoreDelete(s_elog_mutex);
		s_elog_mutex = NULL;
	}

}

/**
 * output log port interface
 *
 * @param log output of log
 * @param size log size
 */
void elog_port_output(const char *log, size_t size) {
    
    /* add your code here */
	size_t i;
	uint32_t timeout;
	for(i = 0;i < size; i++)
	{
		USART_SendData(USART1,log[i]);
		timeout = 0x1FFFF;
		while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
			if(--timeout == 0) return; //带超时时间，等不到就返回
	}
    
}

/**
 * output lock
 */
void elog_port_output_lock(void) {
    
    /* add your code here */
	//调度器开启了
    if(xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED && s_elog_mutex != NULL)
    {
        xSemaphoreTake(s_elog_mutex, pdMS_TO_TICKS(500));
    }
}

/**
 * output unlock
 */
void elog_port_output_unlock(void) {
    
    /* add your code here */
    if(xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED && s_elog_mutex != NULL)
    {
        xSemaphoreGive(s_elog_mutex);
    }
}

/**
 * get current time interface
 *
 * @return current time
 */
const char *elog_port_get_time(void) {
    
    /* add your code here */
    static char buf[16];
	//snprintf开销比较大，自己实现整数转字符串
    //snprintf(buf, sizeof(buf), "%lu", xTaskGetTickCount() * portTICK_PERIOD_MS);
	uint32_t ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
	char *p = buf + sizeof(buf) - 1;
	*p = '\0';
	if(ms == 0){
		
		*--p = '0';
		return p;
	}
	while(ms && p > buf)
	{
		*--p = '0' + (ms %10);
		ms /= 10;
	}
    return p;
}

/**
 * get current process name interface
 *
 * @return current process name
 */
const char *elog_port_get_p_info(void) {
    
    /* add your code here */
    return " ";
}

/**
 * get current thread name interface
 *
 * @return current thread name
 */
const char *elog_port_get_t_info(void) {
    
    /* add your code here */
    if(xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
        return pcTaskGetName(NULL);
    return "main";
}

/**
*	新添加一个，创建锁
**/


void elog_port_mutex_create(void)
{
	if(s_elog_mutex == NULL)
	{
		s_elog_mutex = xSemaphoreCreateMutex();
		if(s_elog_mutex == NULL)
		{
			while(1);
		}
	}
}

/**
 * EasyLogger port initialize
 *
 * @return result
 */
//ElogErrCode elog_port_init(void) {
//    ElogErrCode result = ELOG_NO_ERR;

//    /* add your code here */
//    
//    return result;
//}

///**
// * EasyLogger port deinitialize
// *
// */
//void elog_port_deinit(void) {

//    /* add your code here */
//  if(s_elog_mutex != NULL){
//		vSemaphoreDelete(s_elog_mutex);
//		s_elog_mutex = NULL;
//	}

//}

///**
// * output log port interface
// *
// * @param log output of log
// * @param size log size
// */
//void elog_port_output(const char *log, size_t size) {
//    
//    /* add your code here */
//    
//}

///**
// * output lock
// */
//void elog_port_output_lock(void) {
//    
//    /* add your code here */
//    
//}

///**
// * output unlock
// */
//void elog_port_output_unlock(void) {
//    
//    /* add your code here */
//    
//}

///**
// * get current time interface
// *
// * @return current time
// */
//const char *elog_port_get_time(void) {
//    
//    /* add your code here */
//    
//}

///**
// * get current process name interface
// *
// * @return current process name
// */
//const char *elog_port_get_p_info(void) {
//    
//    /* add your code here */
//    
//}

///**
// * get current thread name interface
// *
// * @return current thread name
// */
//const char *elog_port_get_t_info(void) {
//    
//    /* add your code here */
//    
//}