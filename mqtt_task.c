#define LOG_TAG "mqtt_task"

#include "stm32f4xx.h"
#include "mymqtt.h"
#include "esp01.h"
#include "elog.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>
#include <string.h>
#include "app.h"
#include "http_client.h"
#include "cJSON.h"

/*===========================================================================
 * MQTT 任务
 *
 * 职责：
 *   1. 建立 MQTT 连接、订阅主题
 *   2. 轮询接收下发的消息，解析后投递到事件总线
 *   3. 维护心跳
 *   4. 断线自动重连
 *===========================================================================*/

#define DEV_ID              "stm32_con"

/* 主题定义 */
#define TOPIC_SERVER_CMD	"server/"DEV_ID"/cmd"   /* 订阅服务端的控制命令*/
#define TOPIC_CLIENT_STATUS  "client/"DEV_ID"/status"  /*上报设备状态 */

extern QueueHandle_t Event_Queue;

/*---------------------------------------------------------------------------
 * 建立连接并订阅
 *-------------------------------------------------------------------------*/
static uint8_t mqtt_setup(void)
{
    MQTT_ConnParam_t param;

    param.client_id = DEV_ID;
    param.username  = NULL;         /* 当前 broker 是匿名模式 */
    param.password  = NULL;         
    param.keepalive = 60;

    if(MQTT_Connect(&param, 5000) != MQTT_OK)
    {
        log_e("MQTT connect failed");
        return 1;
    }else{
		log_d("MQTT connect SUCCESS");
	}

    if(MQTT_Subscribe(TOPIC_SERVER_CMD, 0, 3000) != MQTT_OK)
        log_w("subscribe result topic failed");
	else
		log_d("MQTT connect SUCCESS");

    return 0;
}

/*---------------------------------------------------------------------------
 * 处理收到的消息
 *-------------------------------------------------------------------------*/
static void mqtt_handle_message(MQTT_Message_t *msg)
{
    log_i("[%s] %s", msg->topic, msg->payload);

    if(strstr(msg->topic, "/result") != NULL)
    {
        /* 车牌识别结果 -> 投递事件，由分发任务转给显示/蜂鸣器 */
        Event_t evt;
        evt.type      = EVT_PLATE_RESULT;

        //这里用 cJSON 解析 payload，把车牌号等填进 evt
         cJSON *root = cJSON_Parse((char *)msg->payload);
         cJSON *plate = cJSON_GetObjectItem(root, "plate");
         strncpy(evt.data.plate, plate->valuestring, sizeof(evt.data.plate)-1);
         cJSON_Delete(root);
         

        if(xQueueSend(Event_Queue, &evt, 0) != pdTRUE)
            log_w("Event_Queue full");
    }
    else if(strstr(msg->topic, "/cmd") != NULL)
    {
        /* 控制指令 */
    }
}

/*---------------------------------------------------------------------------
 * 上报设备状态
 *-------------------------------------------------------------------------*/
void MQTT_ReportStatus(int16_t temp_x10, uint16_t humi_x10)
{
    char json[160];

    snprintf(json, sizeof(json),
        "{\"devId\":\"%s\",\"timestamp\":%lu,\"temp\":%d.%d,\"humi\":%d.%d}",
        DEV_ID,
        (unsigned long)xTaskGetTickCount(),
        temp_x10 / 10, temp_x10 % 10,
        humi_x10 / 10, humi_x10 % 10);

    MQTT_PublishStr(TOPIC_CLIENT_STATUS, json);
}

/*---------------------------------------------------------------------------
 * 主任务
 *-------------------------------------------------------------------------*/
void MQTT_Task(void *arg)
{
    MQTT_Message_t msg;
    TickType_t last_report = 0;
		Esp01_Init();
    /* 等 ESP01 完成 WiFi 和 TCP 连接 */
    while(!Esp01_IsReady())
        vTaskDelay(pdMS_TO_TICKS(500));

    while(mqtt_setup() != 0)
    {
		g_net_online = 1; //连接正常
        log_w("retry MQTT setup in 5s");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    while(1)
    {
        /* 1. 收消息 —— 一次循环可能有多条，全部取完 */
        while(MQTT_Poll(&msg))
            mqtt_handle_message(&msg);

        /* 2. 心跳 */
        MQTT_KeepAliveTick();

       /* 周期上报状态（30 秒一次） */
        if((xTaskGetTickCount() - last_report) > pdMS_TO_TICKS(30000))
        {
            last_report = xTaskGetTickCount();
            MQTT_ReportStatus(276, 730);    /* 实际接 DHT11 数据 */
        }

        /* 断线检测与重连 */
        if(!MQTT_IsConnected())
        {
			g_net_online = 0;
            log_w("MQTT lost, reconnecting");
            vTaskDelay(pdMS_TO_TICKS(3000));
            if(mqtt_setup() == 0)
			{
				g_net_online = 1;
				HTTP_RetryPending(); //网络恢复断点续传
			}
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}