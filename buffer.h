#ifndef __BUFFER_H__
#define __BUFFER_H__

#include "stm32f4xx.h"

#define ESP01_RX_BUF_SIZE 	2048

/* 返回码 */
#define BUFF_ERROR      0
#define BUFF_OK         1
#define BUFF_FULL       2
#define BUFF_EMPTY      3
#define BUFF_READOK     4
#define BUFF_WRITEOK    5

typedef struct
{
    uint8_t  *buf;              /* 缓冲区指针 */
    uint16_t  size;             /* 缓冲区总容量 */
    volatile uint16_t front;    /* 读指针，任务侧修改 */
    volatile uint16_t rear;     /* 写指针，中断侧修改 */
    volatile uint32_t drop;     /* 溢出丢弃字节数，用于诊断 */
} BufferTypeDef;

typedef struct
{
    uint16_t  size;             /* 改为 uint16_t，支持 >255 字节 */
    uint16_t  length;
    uint8_t  *data;
} BufferClip;

/* 基础操作 */
void     Buffer_Init(BufferTypeDef *buff, uint8_t *mem, uint16_t size);
void     Buffer_Reset(BufferTypeDef *buff);
uint16_t Buffer_Length(BufferTypeDef *buff);
uint16_t Buffer_Free(BufferTypeDef *buff);
uint8_t  Buffer_Push(BufferTypeDef *buff, uint8_t data);
uint8_t  Buffer_Pop(BufferTypeDef *buff, uint8_t *data);
uint16_t Buffer_Pop_All(BufferTypeDef *buff, BufferClip *clip);

/* 协议解析用 */
uint16_t Buffer_Peek(BufferTypeDef *buff, uint16_t offset, uint8_t *data);
int32_t  Buffer_Find(BufferTypeDef *buff, const char *str);
uint16_t Buffer_ReadLen(BufferTypeDef *buff, uint8_t *dst, uint16_t len);
uint16_t Buffer_ReadLine(BufferTypeDef *buff, uint8_t *dst, uint16_t maxlen);
void     Buffer_Discard(BufferTypeDef *buff, uint16_t len);

/* 调试 */
void Buffer_Print(BufferTypeDef *buff);
void Buffer_Clip_Print(BufferClip *clip);

#endif