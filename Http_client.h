#ifndef __HTTP_CLIENT_H__
#define __HTTP_CLIENT_H__

#include "stm32f4xx.h"
#include <stdint.h>

/*===========================================================================
 * HTTP 服务器配置
 *===========================================================================*/
#define HTTP_SERVER_IP      "192.168.0.151"     /* 改成你 PC 的实际 IP */
#define HTTP_SERVER_PORT    8089
#define HTTP_UPLOAD_PATH    "/api/v1/plate/upload"
#define HTTP_DEV_ID         "MaLi-001"

/*
 * 分块大小。ESP01 的 AT+CIPSEND 单次上限约 2048，
 * 取 1024 留余量，避免 AT 指令开销撑爆缓冲。
 */
#define HTTP_CHUNK_SIZE     1024

/* 单块重试次数 */
#define HTTP_CHUNK_RETRY    3

/* 返回码 */
#define HTTP_OK             0
#define HTTP_ERR_CONNECT    1
#define HTTP_ERR_SEND       2
#define HTTP_ERR_RESP       3
#define HTTP_ERR_FILE       4
#define HTTP_ERR_ABORT      5

/*===========================================================================
 * 上传状态（持久化到 Flash，掉电可恢复）
 *===========================================================================*/
typedef struct {
    uint32_t total_size;        /* 文件总字节数 */
    uint32_t sent_offset;       /* 已确认送达的偏移 */
    uint16_t retry_count;       /* 累计重试次数 */
    uint16_t magic;             /* 0xA55A，用于校验文件有效性 */
} HttpUploadState_t;

#define HTTP_STATE_MAGIC    0xA55A

/*===========================================================================
 * API
 *===========================================================================*/

/* 上传指定图片文件，内部自动处理断点续传
 * 返回 HTTP_OK 表示整个文件已确认送达 */
uint8_t HTTP_UploadFile(const char *path, uint32_t img_id);

/* 扫描 Flash 上所有待传文件并依次上传（网络恢复后调用） */
void    HTTP_RetryPending(void);

/* 清除某个文件的上传状态（上传成功后调用） */
void    HTTP_ClearState(uint32_t img_id);

#endif /* __HTTP_CLIENT_H__ */