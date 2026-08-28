#define LOG_TAG "buff"

#include "buffer.h"
#include "elog.h"
#include <string.h>

void Buffer_Init(BufferTypeDef *buff, uint8_t *mem, uint16_t size)
{
    if(buff == NULL || mem == NULL || size == 0) return;

    buff->buf   = mem;
    buff->size  = size;
    buff->front = 0;
    buff->rear  = 0;
    buff->drop  = 0;
}

/* 复位- 读写指针移动到一块，只移读指针，不动写指针，避免与中断竞争 */
void Buffer_Reset(BufferTypeDef *buff)
{
    if(buff == NULL || buff->buf == NULL) return;
    buff->front = buff->rear;
}

uint16_t Buffer_Length(BufferTypeDef *buff)
{
    uint16_t f, r;

    if(buff == NULL || buff->buf == NULL) return 0;

    /* 先取快照 */
    f = buff->front;
    r = buff->rear;

    return (r >= f) ? (r - f) : (buff->size - f + r);
}

uint16_t Buffer_Free(BufferTypeDef *buff)
{
    if(buff == NULL || buff->buf == NULL) return 0;
    return buff->size - 1 - Buffer_Length(buff);
}

/*
 * 满时丢弃新数据，记录丢弃数量。
 */
uint8_t Buffer_Push(BufferTypeDef *buff, uint8_t data)
{
    uint16_t next;

    if(buff == NULL || buff->buf == NULL) return BUFF_ERROR;

    next = buff->rear + 1;
    if(next >= buff->size) next = 0;

    if(next == buff->front) //满了，记录溢出的数据个数，不继续写了
    {
        buff->drop++;
        return BUFF_FULL;
    }

    buff->buf[buff->rear] = data;
    buff->rear = next;                  /* 先写数据后移指针，顺序不能反 */

    return BUFF_WRITEOK;
}

uint8_t Buffer_Pop(BufferTypeDef *buff, uint8_t *data)
{
    if(buff == NULL || buff->buf == NULL || data == NULL) return BUFF_ERROR;
    if(buff->front == buff->rear) return BUFF_EMPTY;

    *data = buff->buf[buff->front];
    buff->front = (buff->front + 1) % (buff->size);

    return BUFF_OK;
}

/* 不消费数据，查看距 front 偏移 offset 处的字节 */
uint16_t Buffer_Peek(BufferTypeDef *buff, uint16_t offset, uint8_t *data)
{
    if(buff == NULL || buff->buf == NULL || data == NULL) return BUFF_ERROR;
    if(offset >= Buffer_Length(buff)) return BUFF_EMPTY;

    *data = buff->buf[(buff->front + offset) % buff->size];
    return BUFF_OK;
}

/* 丢弃 len 个字节 */
void Buffer_Discard(BufferTypeDef *buff, uint16_t len)
{
    uint16_t used;

    if(buff == NULL || buff->buf == NULL) return;

    used = Buffer_Length(buff);
    if(len > used) len = used;

    buff->front = (buff->front + len) % buff->size;
}

/* 精确读取 len 字节，不足则读多少算多少 */
uint16_t Buffer_ReadLen(BufferTypeDef *buff, uint8_t *dst, uint16_t len)
{
    uint16_t used, first;

    if(buff == NULL || buff->buf == NULL || dst == NULL) return 0;

    used = Buffer_Length(buff);  //有效数据长度
    if(len > used) len = used;
    if(len == 0) return 0;

    /* 分两段拷贝，避免逐字节循环 */
    first = buff->size - buff->front;   //处理写跑在读前面的情况
    if(first >= len)
    {
        memcpy(dst, buff->buf + buff->front, len);
    }
    else
    {
        memcpy(dst, buff->buf + buff->front, first);
        memcpy(dst + first, buff->buf, len - first);
    }

    buff->front = (buff->front + len) % buff->size;
    return len;
}

/* 读一行（含 \n），没有完整行返回 0 */
uint16_t Buffer_ReadLine(BufferTypeDef *buff, uint8_t *dst, uint16_t maxlen)
{
    uint16_t used, i, line_len;

    if(buff == NULL || buff->buf == NULL || dst == NULL || maxlen < 2) return 0;

    used = Buffer_Length(buff);

    for(i = 0; i < used; i++)
    {
        if(buff->buf[(buff->front + i) % buff->size] == '\n')
        {
            line_len = i + 1;
            if(line_len > maxlen - 1)
            {
                /* 行太长，丢弃避免卡死 */
                Buffer_Discard(buff, line_len);
                return 0;
            }
            Buffer_ReadLen(buff, dst, line_len);
            dst[line_len] = '\0';
            return line_len;
        }
    }
    return 0;                           /* 还没收到完整行 */
}

uint16_t Buffer_Pop_All(BufferTypeDef *buff, BufferClip *clip)
{
    uint16_t len;

    if(buff == NULL || buff->buf == NULL || clip == NULL || clip->data == NULL)
        return 0;

    len = Buffer_Length(buff);
    if(len == 0) return 0;
    if(len > clip->size) len = clip->size;

    clip->length = Buffer_ReadLen(buff, clip->data, len);
    return clip->length;
}

void Buffer_Print(BufferTypeDef *buff)
{
    uint16_t used, i;

    if(buff == NULL || buff->buf == NULL) return;

    used = Buffer_Length(buff);
    log_d("BUFF[%d,%d] used=%d drop=%lu", buff->front, buff->rear, used, buff->drop);

    for(i = 0; i < used; i++)
       // printf("%c", buff->buf[(buff->front + i) % buff->size]);
		log_i("%c", buff->buf[(buff->front + i) % buff->size]);
    //printf("\r\n");
}

void Buffer_Clip_Print(BufferClip *clip)
{
    uint16_t i;

    if(clip == NULL || clip->data == NULL) return;

    //printf("CLIP[%d]: ", clip->length);
	log_i("CLIP[%d]: ", clip->length);
    for(i = 0; i < clip->length; i++)
        log_i("%c", clip->data[i]);
   // printf("\r\n");
}

/*
 * 查找子串，不消费数据。
 * 返回：>=0 表示在该偏移处找到；-1 表示没找到
 */
int32_t Buffer_Find(BufferTypeDef *buff, const char *str)
{
    uint16_t used, slen, i, j;

    if(buff == NULL || buff->buf == NULL || str == NULL) return -1;
		  
    used = Buffer_Length(buff);
	  
    slen = strlen(str);
		
	if(used >= buff->size)
    {
			
        log_w("buffer corrupted: used=%d size=%d", used, buff->size);
        return -1;
    }
		 
    if(slen == 0 || used < slen) return -1;
		
    for(i = 0; i <= used - slen; i++)
    {
        for(j = 0; j < slen; j++)
        {
            if(buff->buf[(buff->front + i + j) % buff->size] != (uint8_t)str[j])
                  
						break;
        }
        if(j == slen) 
			return (int32_t)i;
    }
    return -1;
}

