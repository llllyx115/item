#ifndef __APP_H__
#define __APP_H__
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "stdint.h"

/* 任务句柄、内核组件 */
 extern TaskHandle_t StartTask_TaskHandle;
 extern SemaphoreHandle_t xFlashMutex;
 extern volatile uint8_t  g_net_online;    /* 0=离线  1=在线 */

typedef enum {
    EVT_CAR_DETECTED,       /* 检测到车辆 */
    EVT_IMAGE_CAPTURED,     /* 图像采集完成 */
    EVT_UPLOAD_SUCCESS,     /* 上传成功 */
    EVT_UPLOAD_FAILED,      /* 上传失败 */
    EVT_PLATE_RESULT,       /* 收到识别结果 */
    EVT_NET_DISCONNECTED,   /* 网络断开 */
    EVT_NET_RECONNECTED,    /* 网络恢复 */
	EVT_SENSOR_DATA			/* 传感器数据  DHT11*/ 
} EventType_t;


typedef struct {
    EventType_t type;
    union {   //联合体 只能有一个成员有效                  
        float    distance;
        uint32_t file_id;
        struct{ 
			uint16_t temp; 
			uint16_t humi;
		} env;
        char  plate[20];  
    }data;
} Event_t;



void Start_Task(void *arg);  //启动任务

void MQTT_Task(void *arg);  //mqtt任务
extern QueueHandle_t Event_Queue;
extern TaskHandle_t Mqtt_TaskHandle;   //mqtt任务
extern SemaphoreHandle_t xUartMutex;

#endif


