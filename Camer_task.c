#define LOG_TAG "camera"

/*===========================================================================
 * Camer_Task.c
 *
 * 数据流：
 *   OV2640 JPEG → DCMI → DMA2 Stream1 双缓冲（buf0/buf1 各 4KB）
 *        ↓ DMA TC 中断（每 4KB 一次）
 *   jpeg_dcmi_rx_callback() → 追加到 s_jpeg_data_buf[]
 *        ↓ DCMI FRAME 中断（每帧一次）
 *   jpeg_data_process()     → 收尾并置 s_jpeg_data_ok=1
 *        ↓ Camera_Task 被信号量唤醒
 *   查找 FF D8 ... FF D9 → 存 Flash → 投事件
 *===========================================================================*/

#include "stm32f4xx.h"
#include "Camer_Task.h"
#include "ov2640.h"
#include "my_dcmi.h"
#include "w25q64.h"
#include "ff.h"
#include "app.h"
#include "elog.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * 缓冲区（uint32_t 数组，实际字节数 = 元素数 × 4）
 *
 *   DMA 双缓冲： 1024 字 × 4 = 4KB，两块共 8KB
 *   累积缓冲：  10240 字 × 4 = 40KB
 *
 * 必须在 SRAM1/SRAM2，不能在 CCM(0x10000000) —— DMA 访问不到 CCM。
 * 编译后在 .map 里搜 s_jpeg_data_buf，地址应为 0x2000xxxx。
 *===========================================================================*/
#define JPEG_DMA_BUF_WORDS    (1  * 1024)
#define JPEG_DATA_BUF_WORDS   (10 * 1024)

static uint32_t __attribute__((aligned(4))) s_dma_buf0[JPEG_DMA_BUF_WORDS];
static uint32_t __attribute__((aligned(4))) s_dma_buf1[JPEG_DMA_BUF_WORDS];
static uint32_t __attribute__((aligned(4))) s_jpeg_data_buf[JPEG_DATA_BUF_WORDS];

/*
 * s_jpeg_data_ok:  0=未完成  1=完成待处理  2=已处理待重采
 * s_jpeg_data_len: 已累积的字数
 */
static volatile uint32_t s_jpeg_data_len = 0;
static volatile uint8_t  s_jpeg_data_ok  = 0;

TaskHandle_t Camera_Task_Handle = NULL;
static SemaphoreHandle_t xFrameDoneSem = NULL;

static QueueHandle_t xCaptureQueue = NULL;

static volatile uint8_t  s_capture_mode = 0;

extern QueueHandle_t Event_Queue;

/*===========================================================================
 * jpeg_data_process
 * 由 DCMI 帧中断调用
 *===========================================================================*/
void jpeg_data_process(void)
{
    uint16_t  rlen, i;
    uint32_t *pbuf;

    if(s_jpeg_data_ok == 0)         /* 当前帧尚未采集完 */
    {
        DMA_Cmd(DMA2_Stream1, DISABLE);
        uint32_t guard = 10000;
		while(DMA_GetCmdStatus(DMA2_Stream1) != DISABLE  && guard--)
		{		
			
		}

        /* 当前块已写入的字数 */
        rlen = JPEG_DMA_BUF_WORDS - DMA_GetCurrDataCounter(DMA2_Stream1);

        pbuf = s_jpeg_data_buf + s_jpeg_data_len;

        /* CT=1 表示 DMA 正在写 M1，那么当前未满的是 M1 */
        if(DMA2_Stream1->CR & (1 << 19))
        {
            for(i = 0; i < rlen; i++) pbuf[i] = s_dma_buf1[i];
        }
        else
        {
            for(i = 0; i < rlen; i++) pbuf[i] = s_dma_buf0[i];
        }

        s_jpeg_data_len += rlen;
        s_jpeg_data_ok   = 1;

        {
            BaseType_t woken = pdFALSE;
            if(xFrameDoneSem != NULL)
                xSemaphoreGiveFromISR(xFrameDoneSem, &woken);
            portYIELD_FROM_ISR(woken);
        }
    }

    if(s_jpeg_data_ok == 2)         /* 上一帧已处理，重新开始 */
    {
        DMA_SetCurrDataCounter(DMA2_Stream1, JPEG_DMA_BUF_WORDS);
        DMA_Cmd(DMA2_Stream1, ENABLE);
        s_jpeg_data_ok  = 0;
        s_jpeg_data_len = 0;
    }
}

/*===========================================================================
 * jpeg_dcmi_rx_callback
 * 由 DMA TC 中断调用，每填满一块 4KB 追加到累积缓冲
 *===========================================================================*/
static void jpeg_dcmi_rx_callback(void)
{
    uint32_t *pbuf;
    uint16_t  i;

    if(s_jpeg_data_ok != 0) return;         /* 当前帧已完成 */

    pbuf = s_jpeg_data_buf + s_jpeg_data_len;

    /* CT=1 表示 DMA 已切到 M1，那么刚填满的是 M0 */
    if(DMA2_Stream1->CR & (1 << 19))
    {
        for(i = 0; i < JPEG_DMA_BUF_WORDS; i++) pbuf[i] = s_dma_buf0[i];
    }
    else
    {
        for(i = 0; i < JPEG_DMA_BUF_WORDS; i++) pbuf[i] = s_dma_buf1[i];
    }

    s_jpeg_data_len += JPEG_DMA_BUF_WORDS;

    /* 防止累积缓冲溢出 */
    if(s_jpeg_data_len >= JPEG_DATA_BUF_WORDS - JPEG_DMA_BUF_WORDS * 2)
    {
        BaseType_t woken = pdFALSE;
        s_jpeg_data_ok = 1;
        if(xFrameDoneSem != NULL)
            xSemaphoreGiveFromISR(xFrameDoneSem, &woken);
        portYIELD_FROM_ISR(woken);
    }
}

/*===========================================================================
 * 对外接口
 *===========================================================================*/
void Camera_TriggerCapture(uint8_t mode)
{
    if(xCaptureQueue == NULL) return;

    if(xQueueSend(xCaptureQueue, &mode, 0) != pdTRUE)
    {
        log_w("capture queue FULL");
        log_w("  waiting=%u, task state=%d",
              (unsigned)uxQueueMessagesWaiting(xCaptureQueue),
              (Camera_Task_Handle != NULL) ?
                  eTaskGetState(Camera_Task_Handle) : -1);
    }
    else
        log_d("capture queued");
}

uint8_t  *Camera_GetFrameBuf(void)  { return (uint8_t *)s_jpeg_data_buf; }
uint32_t  Camera_GetFrameSize(void) { return s_jpeg_data_len * 4; }

/*===========================================================================
 * 存到 W25Q64（FatFs）
 *===========================================================================*/
static uint8_t save_jpeg_to_flash(const uint8_t *buf, uint32_t size,
                                  uint32_t img_id)
{
    static FIL file;
    char    path[32];
    UINT    bw;
    FRESULT fr;

    snprintf(path, sizeof(path), "0:IMG_%03lu.JPG", (unsigned long)img_id);

    fr = f_open(&file, path, FA_CREATE_ALWAYS | FA_WRITE);
    if(fr != FR_OK)
    {
        log_e("f_open %s failed: %d", path, fr);
        return 1;
    }

    fr = f_write(&file, buf, size, &bw);
    f_close(&file);

    if(fr != FR_OK || bw != size)
    {
        log_e("f_write failed: %d, %u/%lu", fr, bw, (unsigned long)size);
        return 1;
    }

    log_i("saved %s (%lu bytes)", path, (unsigned long)bw);
    return 0;
}

/*===========================================================================
 * Camera_Task  完整版
 *
 * 直接替换 Camer_Task.c 里的整个 Camera_Task 函数
 *
 * 关键设计：
 *   1. 每次采集前调 My_DCMI_Init() 完全复位 DCMI，清掉上次残留状态
 *   2. 最多重试 3 次，每次重新初始化，总有一次能落在帧开头
 *   3. 每次拿到帧后必须置 s_jpeg_data_ok = 2，否则中断不会重启 DMA
 *   4. 长度 < 1024 直接判失败重试，不浪费后面的查找流程
 *=========================================================================== */
void Camera_Task(void *arg)
{
    uint32_t img_id = 0;

    log_i("task started");
    xFrameDoneSem = xSemaphoreCreateBinary();
	xCaptureQueue = xQueueCreate(2, sizeof(uint8_t));
	
    if(xCaptureQueue == NULL || xFrameDoneSem == NULL)
    {
        log_e("semaphore create failed");
        vTaskDelete(NULL);
        return;
    }

    /*--- OV2640 初始化 ---*/
    if(OV2640_Init() != 0)
    {
        log_e("OV2640 init FAILED");
        vTaskDelete(NULL);
        return;
    }

    /*--- 格式配置：必须先 YUV422 再 JPEG，顺序不能反 ---*/
    OV2640_JPEG_Mode();

    OV2640_ImageWin_Set(0, 0, 1600, 1200);      /* DSP 输入窗口 = 全尺寸 */
    OV2640_OutSize_Set(640, 480);               /* 输出 VGA */
    vTaskDelay(pdMS_TO_TICKS(500));

    log_i("OV2640 JPEG 640x480 ready");

    /*--- DCMI 初始化 ---*/
    My_DCMI_Init();

    log_i("waiting for capture command...");

    while(1)
    {
		log_d("waiting for trigger..."); 
        uint8_t   mode;
        uint32_t  total_bytes = 0;
        uint8_t  *p;
        uint8_t  *jpeg_start = NULL;
        uint32_t  jpeg_size  = 0;
        uint32_t  i, j;
        uint8_t   k;
        uint8_t   attempt;
        uint8_t   got_frame = 0;

        /*--- 等触发 ---*/
		if(xQueueReceive(xCaptureQueue, &mode, portMAX_DELAY) != pdTRUE)
			continue;
        ///mode = s_capture_mode;
		

        /*=====================================================
         * 采集，最多重试 3 次
         *=====================================================*/
        for(attempt = 0; attempt < 3; attempt++)
        {
            /* 完全复位 DCMI，清掉上次遗留的状态 */
			log_d("att%d: stop", attempt + 1);
            DCMI_Stop();
			log_d("att%d: dcmi init", attempt + 1);
            My_DCMI_Init();

            s_jpeg_data_len = 0;
            s_jpeg_data_ok  = 0;

            dcmi_rx_callback = jpeg_dcmi_rx_callback;
			
			log_d("att%d: dma init", attempt + 1); 
			
            DCMI_DMA_Init((uint32_t)s_dma_buf0,
                          (uint32_t)s_dma_buf1,
                          JPEG_DMA_BUF_WORDS,
                          DMA_MemoryDataSize_Word,
                          DMA_MemoryInc_Enable);
			log_d("att%d: start", attempt + 1);
            DCMI_Start();

            /*--- 丢弃 2 帧暖机 ---*/
            for(k = 0; k < 2; k++)
            {
                if(xSemaphoreTake(xFrameDoneSem, pdMS_TO_TICKS(2000)) != pdTRUE)
                {
                    log_w("warmup %d timeout (attempt %d)", k, attempt + 1);
                    break;
                }

                /* 通知中断重启 DMA，这一步不能省 */
                s_jpeg_data_ok  = 2;
                s_jpeg_data_len = 0;
                vTaskDelay(pdMS_TO_TICKS(50));
            }

            /*--- 正式帧 ---*/
            if(xSemaphoreTake(xFrameDoneSem, pdMS_TO_TICKS(2000)) != pdTRUE)
            {
                log_w("frame timeout, attempt %d", attempt + 1);
                DCMI_Stop();
                s_jpeg_data_ok = 0;
                vTaskDelay(pdMS_TO_TICKS(200));
                continue;
            }

            DCMI_Stop();

            total_bytes = s_jpeg_data_len * 4;

            if(total_bytes >= 1024)
            {
                got_frame = 1;
                break;                          /* 拿到有效帧 */
            }

            log_w("frame too small: %lu, attempt %d",
                  (unsigned long)total_bytes, attempt + 1);

            s_jpeg_data_ok = 0;
            vTaskDelay(pdMS_TO_TICKS(200));
        }

        if(!got_frame)
        {
            log_e("capture failed after 3 attempts");
            s_jpeg_data_ok = 0;
            continue;
        }

        /*=====================================================
         * 解析 JPEG
         *=====================================================*/
        p = (uint8_t *)s_jpeg_data_buf;

        log_i("frame %lu bytes, head: %02X %02X %02X %02X",
              (unsigned long)total_bytes, p[0], p[1], p[2], p[3]);

        jpeg_start = NULL;
        jpeg_size  = 0;

        for(i = 0; i + 1 < total_bytes; i++)
        {
            if(p[i] == 0xFF && p[i+1] == 0xD8)          /* SOI */
            {
                jpeg_start = &p[i];
                for(j = i + 2; j + 1 < total_bytes; j++)
                {
                    if(p[j] == 0xFF && p[j+1] == 0xD9)  /* EOI */
                    {
                        jpeg_size = j + 2 - i;
                        break;
                    }
                }
                break;
            }
        }

        if(jpeg_start == NULL || jpeg_size < 4)
        {
            log_w("no JPEG SOI/EOI in %lu bytes", (unsigned long)total_bytes);
            s_jpeg_data_ok = 0;
            continue;
        }

        log_i("JPEG OK offset=%lu size=%lu",
              (unsigned long)(jpeg_start - p), (unsigned long)jpeg_size);

        /*=====================================================
         * 按模式处理
         *=====================================================*/
        if(mode == CAM_MODE_TEST)
        {
            /* 只验证，不落盘 */
        }
        else
        {
            if(save_jpeg_to_flash(jpeg_start, jpeg_size, img_id) == 0)
            {
                Event_t evt = {0};              /* 联合体必须清零 */
                evt.type         = EVT_IMAGE_CAPTURED;
                evt.data.file_id = img_id;

                if(xQueueSend(Event_Queue, &evt, 0) != pdTRUE)
                    log_w("Event_Queue full");

                img_id++;
            }
        }

        s_jpeg_data_ok = 0;
    }
}

