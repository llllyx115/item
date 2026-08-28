#ifndef __CAMERA_TASK_H__
#define __CAMERA_TASK_H__

#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>

/* 抓拍模式 */
#define CAM_MODE_UPLOAD     0   /* 存 Flash 并触发上传 */
#define CAM_MODE_TEST       1   /* 只采集，用于调试 */

extern TaskHandle_t Camera_Task_Handle;

void      Camera_Task(void *arg);
void      Camera_TriggerCapture(uint8_t mode);
uint8_t  *Camera_GetFrameBuf(void);
uint32_t  Camera_GetFrameSize(void);

/* 供 DCMI 帧中断调用 */
void      jpeg_data_process(void);

#endif /* __CAMERA_TASK_H__ */