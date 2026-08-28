#define LOG_TAG "http"

/*===========================================================================
 * http_client.c
 * 通过 ESP01 AT 指令实现分块 HTTP POST + 断点续传
 *
 * 续传原理：
 *   每个图片配一个同名 .ST 状态文件，记录已确认送达的偏移。
 *   上传前读状态文件，从 sent_offset 继续；每块确认后更新状态文件。
 *   掉电或断网重启后，状态仍在 Flash 上，可以接着传。
 *
 * 与服务端的约定：
 *   请求头带 X-Chunk-Offset 和 X-Total-Size，
 *   服务端按 offset 写入文件对应位置，收齐后返回 "COMPLETE"。
 *===========================================================================*/

#include "stm32f4xx.h"
#include "http_client.h"
#include "esp01.h"
#include "loopBuff.h"
#include "ff.h"
#include "app.h"
#include "elog.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <string.h>
#include <stdio.h>
#include "app.h"
#include "mymqtt.h"


extern BufferTypeDef     g_esp01_rx;
extern SemaphoreHandle_t xFlashMutex;
extern volatile uint8_t  g_net_online;

/* 分块读取用的缓冲。static 避免占栈 */
static uint8_t s_chunk_buf[HTTP_CHUNK_SIZE];
static char    s_at_cmd[128];
static char    s_header[320];

/*===========================================================================
 * 状态文件读写
 *===========================================================================*/
static void state_path(char *out, uint16_t len, uint32_t img_id)
{
    snprintf(out, len, "0:IMG_%03lu.ST", (unsigned long)img_id);
}

static uint8_t state_load(uint32_t img_id, HttpUploadState_t *st)
{
    FIL     f;
    UINT    br;
    char    path[32];
    FRESULT fr;

    state_path(path, sizeof(path), img_id);

    fr = f_open(&f, path, FA_READ);
    if(fr != FR_OK) return 1;                   /* 没有状态文件 = 首次上传 */

    fr = f_read(&f, st, sizeof(HttpUploadState_t), &br);
    f_close(&f);

    if(fr != FR_OK || br != sizeof(HttpUploadState_t)) return 1;
    if(st->magic != HTTP_STATE_MAGIC)           return 1;   /* 文件损坏 */

    return 0;
}

static uint8_t state_save(uint32_t img_id, const HttpUploadState_t *st)
{
    FIL     f;
    UINT    bw;
    char    path[32];
    FRESULT fr;

    state_path(path, sizeof(path), img_id);

    fr = f_open(&f, path, FA_CREATE_ALWAYS | FA_WRITE);
    if(fr != FR_OK) return 1;

    fr = f_write(&f, st, sizeof(HttpUploadState_t), &bw);
    f_close(&f);

    return (fr == FR_OK && bw == sizeof(HttpUploadState_t)) ? 0 : 1;
}

void HTTP_ClearState(uint32_t img_id)
{
    char path[32];
    state_path(path, sizeof(path), img_id);
    f_unlink(path);
}

/*===========================================================================
 * 退出透传 / 恢复透传
 *
 * MQTT 走透传模式常驻，HTTP 上传要临时切出去用非透传的 AT+CIPSEND=<len>。
 *===========================================================================*/
static uint8_t exit_transparent(void)
{
	uint8_t retry;

    for(retry = 0; retry < 3; retry++)
    {
        if(ESP01_SendCmd("AT\r\n", "OK", 500) == 0)
        {
            log_d("in AT mode");
            return 0;
        }

        log_d("try +++ (%d)", retry + 1);
        vTaskDelay(pdMS_TO_TICKS(1100));
        Usart2_SendString((uint8_t *)"+++");
        vTaskDelay(pdMS_TO_TICKS(1100));
    }

    log_e("exit transparent failed after 3 tries");
    return 1;
}

static uint8_t restore_transparent(void)
{
    extern char *at_cipstart;

    ESP01_SendCmd("AT+CIPMUX=0\r\n",  "OK", 1000);
    ESP01_SendCmd("AT+CIPMODE=1\r\n", "OK", 1000);

    if(ESP01_SendCmd(at_cipstart, "CONNECT", 8000) != 0) return 1;
    if(ESP01_SendCmd("AT+CIPSEND\r\n", ">", 2000)  != 0) return 2;

    return 0;
}

/*===========================================================================
 * 重试发送一块数据
 * 返回 0=成功
 *===========================================================================*/
static uint8_t send_chunk(const uint8_t *data, uint16_t len)
{
    snprintf(s_at_cmd, sizeof(s_at_cmd), "AT+CIPSEND=%u\r\n", (unsigned)len);

    /* 只有拿不到 '>' 提示符时才能安全重试——那说明数据根本没发 */
    uint8_t retry;
    for(retry = 0; retry < HTTP_CHUNK_RETRY; retry++)
    {
        if(ESP01_SendCmd(s_at_cmd, ">", 3000) == 0) break;
        log_w("CIPSEND prompt timeout, retry %d", retry + 1);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    if(retry >= HTTP_CHUNK_RETRY) return 1;

    Usart2_SendBuf((uint8_t *)data, len);

    /* 数据已经出去了，SEND OK 超时也不能重发 */
    if(ESP01_WaitAck("SEND OK", 8000) != 0)
    {
        log_e("SEND OK timeout after data sent, abort");
        return 1;
    }

    return 0;
}

/*===========================================================================
 * HTTP_UploadFile
 *===========================================================================*/
uint8_t HTTP_UploadFile(const char *path, uint32_t img_id)
{
    static FIL     file;
    FRESULT fr;
    UINT    br;
    HttpUploadState_t st;
    uint8_t  ret = HTTP_OK;
    uint8_t  first_time;
    uint32_t file_size;
    int      header_len;
	if(xSemaphoreTake(xUartMutex, portMAX_DELAY) != pdTRUE)
    return HTTP_ERR_CONNECT;

	if(Mqtt_TaskHandle != NULL)
    vTaskSuspend(Mqtt_TaskHandle);
    /*--- 打开文件，取大小 ---*/
    if(xSemaphoreTake(xFlashMutex, pdMS_TO_TICKS(5000)) != pdTRUE)
    {
        log_e("flash mutex timeout");
        return HTTP_ERR_FILE;
    }

    fr = f_open(&file, path, FA_READ);
    if(fr != FR_OK)
    {
        log_e("f_open %s failed: %d", path, fr);
        xSemaphoreGive(xFlashMutex);
        return HTTP_ERR_FILE;
    }
    file_size = f_size(&file);

    /*--- 读续传状态 ---*/
    first_time = state_load(img_id, &st);
    if(first_time)
    {
        st.total_size  = file_size;
        st.sent_offset = 0;
        st.retry_count = 0;
        st.magic       = HTTP_STATE_MAGIC;
        log_i("upload %s, size=%lu (new)", path, (unsigned long)file_size);
    }
    else
    {
        if(st.total_size != file_size)          /* 文件变了，状态作废 */
        {
            st.total_size  = file_size;
            st.sent_offset = 0;
        }
        log_i("upload %s, resume from %lu/%lu",
              path, (unsigned long)st.sent_offset, (unsigned long)file_size);
    }

    if(st.sent_offset >= file_size)             /* 已经传完了 */
    {
        f_close(&file);
        xSemaphoreGive(xFlashMutex);
        HTTP_ClearState(img_id);
        return HTTP_OK;
    }

    f_close(&file);
    xSemaphoreGive(xFlashMutex);
	
	log_d("step1: exit transparent"); 
	
	/* 独占串口：挂起 MQTT 任务 */
    if(Mqtt_TaskHandle != NULL)
        vTaskSuspend(Mqtt_TaskHandle);
	
    /*--- 切出透传，建立 HTTP 连接 ---*/
    if(exit_transparent() != 0)
	{
		ret = HTTP_ERR_CONNECT;
		goto resume;

	}		
	log_d("step2: close old TCP"); 
	
    ESP01_SendCmd("AT+CIPCLOSE\r\n", "OK", 2000);
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP01_SendCmd("AT+CIPMUX=0\r\n",  "OK", 1000);
    ESP01_SendCmd("AT+CIPMODE=0\r\n", "OK", 1000);

    snprintf(s_at_cmd, sizeof(s_at_cmd),
             "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n",
             HTTP_SERVER_IP, HTTP_SERVER_PORT);

    if(ESP01_SendCmd(s_at_cmd, "CONNECT", 8000) != 0)
    {
        log_e("TCP connect failed");
        ret = HTTP_ERR_CONNECT;
        goto restore;
    }
log_d("step3: connect http server");
	
    /*--- 组 HTTP 头 ---
     * Content-Length 是"本次要发的剩余字节数"，
     * X-Chunk-Offset 告诉服务端从文件哪个位置开始写。
     */
    header_len = snprintf(s_header, sizeof(s_header),
        "POST %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Length: %lu\r\n"
        "X-Device-Id: %s\r\n"
        "X-Img-Id: %lu\r\n"
        "X-Chunk-Offset: %lu\r\n"
        "X-Total-Size: %lu\r\n"
        "Connection: close\r\n\r\n",
        HTTP_UPLOAD_PATH,
        HTTP_SERVER_IP, HTTP_SERVER_PORT,
        (unsigned long)(file_size - st.sent_offset),
        HTTP_DEV_ID,
        (unsigned long)img_id,
        (unsigned long)st.sent_offset,
        (unsigned long)file_size);

    if(send_chunk((uint8_t *)s_header, (uint16_t)header_len) != 0)
    {
        log_e("send header failed");
        ret = HTTP_ERR_SEND;
        goto close;
    }
log_d("step4: send header");

    /*--- 分块发送文件体 ---*/
    while(st.sent_offset < file_size)
    {
        uint32_t remain = file_size - st.sent_offset;
        uint16_t chunk  = (remain > HTTP_CHUNK_SIZE)
                          ? HTTP_CHUNK_SIZE : (uint16_t)remain;

        /* 从 Flash 读一块 */
        if(xSemaphoreTake(xFlashMutex, pdMS_TO_TICKS(5000)) != pdTRUE)
        {
            ret = HTTP_ERR_FILE;
            goto close;
        }

        fr = f_open(&file, path, FA_READ);
        if(fr == FR_OK)
        {
            f_lseek(&file, st.sent_offset);
            fr = f_read(&file, s_chunk_buf, chunk, &br);
            f_close(&file);
        }
        xSemaphoreGive(xFlashMutex);

        if(fr != FR_OK || br != chunk)
        {
            log_e("f_read failed at %lu", (unsigned long)st.sent_offset);
            ret = HTTP_ERR_FILE;
            goto close;
        }

        /* 发出去 */
        if(send_chunk(s_chunk_buf, chunk) != 0)
        {
            st.retry_count++;
            state_save(img_id, &st);            /* 保存进度，下次接着传 */
            log_e("chunk failed at %lu, saved progress",
                  (unsigned long)st.sent_offset);
            ret = HTTP_ERR_SEND;
            goto close;
        }

        st.sent_offset += chunk;

        /* 每块都落盘状态。 */
        state_save(img_id, &st);

        log_d("sent %lu/%lu",
              (unsigned long)st.sent_offset, (unsigned long)file_size);

        vTaskDelay(pdMS_TO_TICKS(10));          /* 让出 CPU */
    }
	log_d("step5: send body");
    /*--- 等服务端响应 ---*/
    if(ESP01_WaitAck("COMPLETE", 8000) == 0)
    {
        log_i("upload %s COMPLETE", path);
        HTTP_ClearState(img_id);
        ret = HTTP_OK;
    }
    else if(Buffer_Find(&g_esp01_rx, "200 OK") >= 0)
    {
        log_i("upload %s accepted (partial)", path);
        ret = HTTP_OK;
    }
    else
    {
        log_w("no valid response");
        ret = HTTP_ERR_RESP;
    }

close:
    ESP01_SendCmd("AT+CIPCLOSE\r\n", "OK", 2000);
    vTaskDelay(pdMS_TO_TICKS(100));

restore:
    if(restore_transparent() != 0)
    {
        log_e("restore MQTT failed, mark offline");
        g_net_online = 0;
    }else{
		/* TCP 断过，broker 那边已掉线，要重发 CONNECT */
        MQTT_ConnParam_t p = { .client_id = "MaLi-001", .username = NULL,
                               .password = NULL, .keepalive = 60 };
        MQTT_Connect(&p, 5000);
        MQTT_Subscribe("server/MaLi-001/cmd", 0, 3000);
	}
resume:
    if(Mqtt_TaskHandle != NULL)
        vTaskResume(Mqtt_TaskHandle);
    xSemaphoreGive(xUartMutex);
    return ret;
}

/*===========================================================================
 * 扫描并重传所有未完成的文件
 * 网络恢复时调用
 *===========================================================================*/
void HTTP_RetryPending(void)
{
    DIR     dir;
    FILINFO fno;
    FRESULT fr;
    char    img_path[32];
    uint32_t img_id;

    if(xSemaphoreTake(xFlashMutex, pdMS_TO_TICKS(5000)) != pdTRUE) return;
    fr = f_opendir(&dir, "0:/");
    xSemaphoreGive(xFlashMutex);

    if(fr != FR_OK)
    {
        log_e("opendir failed: %d", fr);
        return;
    }

    while(1)
    {
        if(xSemaphoreTake(xFlashMutex, pdMS_TO_TICKS(5000)) != pdTRUE) break;
        fr = f_readdir(&dir, &fno);
        xSemaphoreGive(xFlashMutex);

        if(fr != FR_OK || fno.fname[0] == 0) break;

        /* 找 .ST 状态文件，说明这个图片没传完 */
        if(strstr(fno.fname, ".ST") == NULL) continue;

        /* IMG_007.ST -> 提取 7 */
        if(sscanf(fno.fname, "IMG_%lu.ST", (unsigned long *)&img_id) != 1)
            continue;

        snprintf(img_path, sizeof(img_path), "0:IMG_%03lu.JPG",
                 (unsigned long)img_id);

        log_i("resume pending: %s", img_path);

        if(HTTP_UploadFile(img_path, img_id) != HTTP_OK)
        {
            log_w("resume failed, will retry later");
            break;                              /* 网络还没好，下次再来 */
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }

    if(xSemaphoreTake(xFlashMutex, pdMS_TO_TICKS(5000)) == pdTRUE)
    {
        f_closedir(&dir);
        xSemaphoreGive(xFlashMutex);
    }
}