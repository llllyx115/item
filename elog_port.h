#ifndef _ELOG_PORT_H_
#define _ELOG_PORT_H_

#include "stm32f4xx.h"                  // Device header
#include <elog.h>
#include "FreeRTOS.h"
#include "semphr.h"


ElogErrCode elog_port_init(void);
void elog_port_deinit(void);
void elog_port_output(const char *log, size_t size);
void elog_port_output_lock(void);
void elog_port_output_unlock(void);
const char *elog_port_get_time(void);
const char *elog_port_get_p_info(void);
const char *elog_port_get_t_info(void);
void elog_port_mutex_create(void);

#endif

