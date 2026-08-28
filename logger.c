#include "stm32f4xx.h"                  // Device header
#include <elog.h>
#include "FreeRTOS.h"
#include "semphr.h"

static SemaphoreHandle_t s_elog_mutex = NULL;  //互斥锁

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

