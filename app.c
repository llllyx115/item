#define LOG_TAG "app"
#include "stm32f4xx.h"
#include "app.h"
#include "myled.h"
#include "stdio.h"
#include "FreeRTOS.h"
#include "ff.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "mydht11.h"
#include "mytime.h"
#include "elog.h"
#include "esp01.h"
#include "buffer.h"
#include "Camer_task.h"
#include "Ov2640.h"
#include "w25q64.h"
#include "Http_client.h"

extern BufferTypeDef g_esp01_rx;
extern SemaphoreHandle_t g_esp01_rx_sem;

TaskHandle_t StartTask_TaskHandle = NULL;
TaskHandle_t DHT11_TaskHandle = NULL;
TaskHandle_t SRF05_TaskHandle = NULL;
TaskHandle_t Task3_TaskHandle = NULL;
TaskHandle_t Event_Dispatch_TaskHandle = NULL;
TaskHandle_t Mqtt_TaskHandle = NULL;
TaskHandle_t Upload_TaskHandle = NULL;

QueueHandle_t Upload_Queue = NULL;
QueueHandle_t SRF05_Queue = NULL;
QueueHandle_t Event_Queue = NULL;

SemaphoreHandle_t xFlashMutex = NULL;
SemaphoreHandle_t xUartMutex = NULL;

volatile uint8_t g_net_online = 0;

/*===========================================================================
 * Upload_Task —— 从 Flash 读图片并通过 HTTP 上传
 *
 * 【修复】每一步都加日志，精确定位卡在哪一环
 *===========================================================================*/
void Upload_Task(void *arg)
{
    uint32_t img_id;
    char path[32];

    log_i("Upload_Task started, waiting for jobs...");

    while(1)
    {
        log_d("upload: waiting on queue...");
        if(xQueueReceive(Upload_Queue, &img_id, portMAX_DELAY) != pdTRUE)
        {
            log_w("upload: xQueueReceive failed");
            continue;
        }

        log_i("upload: GOT job id=%lu", (unsigned long)img_id);

        snprintf(path, sizeof(path), "0:IMG_%03lu.JPG", (unsigned long)img_id);
        log_i("upload: path=%s", path);

        /* 上传前确认文件存在且大小正确 */
        {
            FIL f;
            if(f_open(&f, path, FA_READ) == FR_OK)
            {
                log_i("upload: file exists, size=%lu", (unsigned long)f_size(&f));
                f_close(&f);
            }
            else
            {
                log_e("upload: file NOT found: %s", path);
            }
        }

        log_i("upload: calling HTTP_UploadFile...");
        HTTP_UploadFile(path, img_id);
        log_i("upload: HTTP_UploadFile returned");
    }
}

/*===========================================================================
 * DHT11 任务
 *===========================================================================*/
void DHT11_FUN(void *arg)
{
	DHT11_Data_t d;
	uint8_t ret;
	DHT11_Init();
	log_i("DHT11 init ok");
	while(1)
	{
		vTaskDelay(pdMS_TO_TICKS(2000));
		ret = DHT11_Read(&d);
		if(ret == 0)
		{
			log_i("T=%d.%d H=%d.%d", d.temp_int, d.temp_dec, d.humi_int, d.humi_dec);
			Event_t evt = {
				.type = EVT_SENSOR_DATA,
				.data.env.temp = d.temp_int * 10 + d.temp_dec,
				.data.env.humi = d.humi_int * 10 + d.humi_dec
			};
			xQueueSend(Event_Queue, &evt, 0);
		}
		else{
			log_e("DHT11 error: %d", ret);
		}
	}
}

/*===========================================================================
 * SRF05 测距任务
 *===========================================================================*/
void SRF05_FUN(void *arg)
{
    SRF05_Data_t d;
    uint8_t car_present = 0;
    Srf05_Init();

    extern volatile uint8_t g_srf05_wait_echo;

    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(1200));
        Srf05_Start();

        log_d("before recv data");

        if(xQueueReceive(SRF05_Queue, &d, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            float dist = d.time_us * 0.034f / 2.0f;
            log_i("tim=%dus dist=%.2fcm", d.time_us, dist);

            if(dist > 30.0f && dist < 60.0f)
            {
                if(!car_present)
                {
                    car_present = 1;
                    log_i("car detected, trigger capture");
                    Event_t evt = {0};
                    evt.type = EVT_CAR_DETECTED;
                    evt.data.distance = dist;
                    xQueueSend(Event_Queue, &evt, 0);
                }
            }
            else if(dist > 60.0f)
            {
                if(car_present)
                {
                    car_present = 0;
                    log_i("car left");
                }
            }
        }
        else
        {
            log_d("SRF05 no echo");
            g_srf05_wait_echo = 0;
            TIM_Cmd(TIM6, DISABLE);
        }
    }
}

/*===========================================================================
 * Event_Dispatch
 *===========================================================================*/
void Event_Dispatch_FUN(void *arg)
{
	Event_t evt;
	while(1)
	{
		if(xQueueReceive(Event_Queue, &evt, portMAX_DELAY) != pdTRUE)
            continue;

		switch(evt.type)
		{
			case EVT_CAR_DETECTED:
				log_i("car detected%.1fcm, trigger capture", evt.data.distance);
				Camera_TriggerCapture(CAM_MODE_UPLOAD);
			break;

			case EVT_IMAGE_CAPTURED:
			{
				uint32_t id = evt.data.file_id;
				log_i("image saved, id=%lu, sending to Upload_Queue", (unsigned long)id);
				if(xQueueSend(Upload_Queue, &id, 0) != pdTRUE)
					log_e("Upload_Queue FULL, id=%lu dropped!", (unsigned long)id);
				else
					log_i("sent to Upload_Queue ok");
			}
			break;

			case EVT_UPLOAD_SUCCESS:
                log_i("upload success event");
			break;

			case EVT_UPLOAD_FAILED:
                log_e("upload failed event");
            break;

			case EVT_PLATE_RESULT:
            break;

			case EVT_NET_DISCONNECTED:
			case EVT_NET_RECONNECTED:
				HTTP_RetryPending();
            break;

			case EVT_SENSOR_DATA:
			{
				uint16_t t = evt.data.env.temp;
				uint16_t h = evt.data.env.humi;
				log_i("T=%d.%dC  H=%d.%d %%", t/10, t%10, h/10, h%10);
				break;
			}

			default:
            break;
		}
	}
}

/*===========================================================================
 * Start_Task
 *===========================================================================*/
void Start_Task(void *arg)
{
	BaseType_t xReturn;

	/***************** 内核对象 *****************/
	SRF05_Queue = xQueueCreate(4, sizeof(SRF05_Data_t));
	if(SRF05_Queue != NULL) log_i("SRF05_Queue create OK"); else log_e("SRF05_Queue create FAILED");

	Event_Queue = xQueueCreate(15, sizeof(Event_t));
	if(Event_Queue != NULL) log_i("Event_Queue create OK"); else log_e("Event_Queue create FAILED");

	Upload_Queue = xQueueCreate(4, sizeof(uint32_t));
	if(Upload_Queue != NULL) log_i("Upload_Queue create OK"); else log_e("Upload_Queue create FAILED");

	elog_port_mutex_create();

	xFlashMutex = xSemaphoreCreateMutex();
	if(xFlashMutex != NULL) log_i("xFlashMutex create OK"); else log_e("xFlashMutex create FAILED");

	xUartMutex = xSemaphoreCreateMutex();

	/***************** FatFs 初始化 *****************/
	log_i("FatFs setup start...");
	if(FatFs_Setup() != 0)
	{
		log_e("FatFs setup FAILED");
	}
	else
	{
		log_i("FatFs setup OK");
	}

	/**************** DHT11 ***********************/
	xReturn = xTaskCreate((TaskFunction_t) DHT11_FUN, "DHT11_FUN",
		512, NULL, 2, &DHT11_TaskHandle);
	if(xReturn == pdPASS) log_i("DHT11 task create OK"); else log_e("DHT11 create FAILED");

	/**************** SRF05 ***********************/
	xReturn = xTaskCreate((TaskFunction_t) SRF05_FUN, "SRF05_FUN",
		512, NULL, 2, &SRF05_TaskHandle);
	if(xReturn == pdPASS) log_i("SRF05 create OK"); else log_e("SRF05 create FAILED");

	/**************** Event_Dispatch ***********************/
	xReturn = xTaskCreate((TaskFunction_t) Event_Dispatch_FUN, "Event_Dispatch_FUN",
		512, NULL, 6, &Event_Dispatch_TaskHandle);
	if(xReturn == pdPASS) log_i("Event_Dispatch create OK"); else log_e("Event_Dispatch create FAILED");

	/**************** Mqtt_Task ***********************/
	xReturn = xTaskCreate((TaskFunction_t) MQTT_Task, "MQTT_Task",
		1024, NULL, 4, &Mqtt_TaskHandle);
	if(xReturn == pdPASS) log_i("MQTT_Task create OK"); else log_e("MQTT_Task create FAILED");

	/**************** Camera_Task ***********************/
	xReturn = xTaskCreate((TaskFunction_t) Camera_Task, "Camera_Task",
		4096, NULL, 7, &Camera_Task_Handle);
	if(xReturn == pdPASS)
	{
		log_i("Camera_Task create OK");
		vTaskSuspend(Camera_Task_Handle);
	}
	else log_e("Camera_Task create FAILED");

	/**************** Upload_Task ***********************/
	xReturn = xTaskCreate(Upload_Task, "Upload_Task", 2048, NULL, 3, &Upload_TaskHandle);
	if(xReturn == pdPASS) log_i("Upload_Task create OK"); else log_e("Upload_Task create FAILED");

	/* 恢复 Camera 任务 */
	if(Camera_Task_Handle != NULL)
	{
		log_i("resume Camera_Task");
		vTaskResume(Camera_Task_Handle);
	}

	log_i("Start_Task done, deleting itself");
	vTaskDelete(NULL);
}
