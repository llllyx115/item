#define LOG_TAG "mqtt"

#include "stm32f4xx.h"
#include "mymqtt.h"
#include "esp01.h"
#include "buffer.h"
#include "elog.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>


/*===========================================================================
 * 内部状态
 *===========================================================================*/
static uint8_t  s_tx_buf[MQTT_TX_BUF_SIZE];
static uint16_t s_packet_id = 1;            
static uint8_t  s_connected = 0;
static TickType_t s_last_send_tick = 0;

extern BufferTypeDef g_esp01_rx;          

/*===========================================================================
 * 工具函数
 *===========================================================================*/

/* 写入 "2字节大端长度 + 内容"，返回写入的总字节数 */
static uint16_t mqtt_write_string(uint8_t *buf, const char *str)
{
    uint16_t len = strlen(str);

    buf[0] = (uint8_t)(len >> 8);
    buf[1] = (uint8_t)(len & 0xFF);
    memcpy(&buf[2], str, len);

    return len + 2;
}

/*
 * 编码剩余长度字段（变长，1~4 字节）
 */
static uint8_t mqtt_encode_len(uint8_t *buf, uint32_t len)
{
    uint8_t i = 0;

    do {
        uint8_t b = len % 128;
        len /= 128;
        if(len > 0) b |= 0x80;      /* 还有后续字节 */
        buf[i++] = b;
    } while(len > 0 && i < 4);

    return i;
}

/*
 * 解码剩余长度字段
 */
static uint8_t mqtt_decode_len(uint16_t offset, uint32_t *out_len)
{
    uint32_t multiplier = 1;
    uint32_t value = 0;
    uint8_t  i = 0;
    uint8_t  b;

    do {
        if(Buffer_Peek(&g_esp01_rx, offset + i, &b) != BUFF_OK)
            return 0;               /* 数据不够，等下次 */

        value += (b & 0x7F) * multiplier;
        multiplier *= 128;
        i++;

        if(i > 4) return 0;         /* 格式非法 */

    } while((b & 0x80) != 0);

    *out_len = value;
    return i;
}

/* 发送已组好的报文 */
static void mqtt_send(uint8_t *buf, uint16_t len)
{
    Usart2_SendBuf(buf, len);
    s_last_send_tick = xTaskGetTickCount();
}

/*===========================================================================
 * CONNECT
 *===========================================================================*/
uint8_t MQTT_Connect(const MQTT_ConnParam_t *param, uint32_t timeout_ms)
{
    uint16_t i = 0;
    uint8_t  len_bytes[4];
    uint8_t  len_size;
    uint32_t remaining = 0;
    uint8_t  connect_flags = 0x02;          /* 没有用户名和密码 */
    uint16_t keepalive;
    TickType_t start;

    if(param == NULL || param->client_id == NULL)
        return MQTT_ERR_PARAM;

    keepalive = (param->keepalive == 0) ? MQTT_KEEPALIVE_SEC : param->keepalive;

    /*--- 先算剩余长度 ---*/
    remaining  = 10;                                    /* 可变报头固定 10 字节 */
    remaining += 2 + strlen(param->client_id);          /* Client ID */

    if(param->username != NULL)
    {
        connect_flags |= 0x80;                          /* bit7: User Name Flag */
        remaining += 2 + strlen(param->username);
    }
    if(param->password != NULL)
    {
        connect_flags |= 0x40;                          /* bit6: Password Flag */
        remaining += 2 + strlen(param->password);
    }

    /*--- 固定报头 ---*/
    s_tx_buf[i++] = MQTT_CONNECT;
    len_size = mqtt_encode_len(len_bytes, remaining);
    memcpy(&s_tx_buf[i], len_bytes, len_size);
    i += len_size;

    /*--- 可变报头 ---*/
    i += mqtt_write_string(&s_tx_buf[i], "MQTT");       /* 协议名 */
    s_tx_buf[i++] = 0x04;                               /* 协议级别 4 = 3.1.1 */
    s_tx_buf[i++] = connect_flags;
    s_tx_buf[i++] = (uint8_t)(keepalive >> 8);
    s_tx_buf[i++] = (uint8_t)(keepalive & 0xFF);

    /*--- Payload：顺序固定，不能颠倒 ---*/
    i += mqtt_write_string(&s_tx_buf[i], param->client_id);
    if(param->username != NULL)
        i += mqtt_write_string(&s_tx_buf[i], param->username);
    if(param->password != NULL)
        i += mqtt_write_string(&s_tx_buf[i], param->password);

    /*--- 发送 ---*/
    Buffer_Reset(&g_esp01_rx);
    mqtt_send(s_tx_buf, i);
    log_d("CONNECT sent, %d bytes", i);

    /*--- 等 CONNACK: 20 02 <flags> <retcode> ---*/
    start = xTaskGetTickCount();
    while((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms))
    {
        if(Buffer_Length(&g_esp01_rx) >= 4)
        {
            uint8_t b0, b1, b3;

            Buffer_Peek(&g_esp01_rx, 0, &b0);
            Buffer_Peek(&g_esp01_rx, 1, &b1);
            Buffer_Peek(&g_esp01_rx, 3, &b3);

            if(b0 == MQTT_CONNACK && b1 == 0x02)
            {
                Buffer_Discard(&g_esp01_rx, 4);

                if(b3 == MQTT_CONNACK_OK)
                {
                    s_connected = 1;
                    log_i("MQTT connected");
                    return MQTT_OK;
                }

                log_e("CONNACK refused, code=%d", b3);
                return MQTT_ERR_REFUSED;
            }

            /* 头字节不对，丢一个字节重新对齐 */
            Buffer_Discard(&g_esp01_rx, 1);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    log_w("CONNACK timeout");
    return MQTT_ERR_TIMEOUT;
}

/*===========================================================================
 * PUBLISH
 *===========================================================================*/
uint8_t MQTT_Publish(const char *topic, const uint8_t *payload, uint16_t len, uint8_t qos)
{
    uint16_t i = 0;
    uint8_t  len_bytes[4];
    uint8_t  len_size;
    uint32_t remaining;

    if(topic == NULL) return MQTT_ERR_PARAM;
    if(!s_connected)  return MQTT_ERR_REFUSED;

    remaining = 2 + strlen(topic) + len;
    if(qos > 0) remaining += 2;                 /* QoS>0 才有报文标识符 */

    if(remaining + 5 > MQTT_TX_BUF_SIZE)
    {
        log_e("publish too large: %lu", remaining);
        return MQTT_ERR_BUFFER;
    }

    /*--- 固定报头 ---*/
    s_tx_buf[i++] = MQTT_PUBLISH | ((qos & 0x03) << 1);
    len_size = mqtt_encode_len(len_bytes, remaining);
    memcpy(&s_tx_buf[i], len_bytes, len_size);
    i += len_size;

    /*--- 可变报头 ---*/
    i += mqtt_write_string(&s_tx_buf[i], topic);

    if(qos > 0)
    {
        s_tx_buf[i++] = (uint8_t)(s_packet_id >> 8);
        s_tx_buf[i++] = (uint8_t)(s_packet_id & 0xFF);
        s_packet_id++;
        if(s_packet_id == 0) s_packet_id = 1;   /* 0 是保留值 */
    }

    /*--- Payload ---*/
    if(payload != NULL && len > 0)
    {
        memcpy(&s_tx_buf[i], payload, len);
        i += len;
    }

    mqtt_send(s_tx_buf, i);
    log_d("PUB [%s] %d bytes", topic, len);

    return MQTT_OK;
}

uint8_t MQTT_PublishStr(const char *topic, const char *str)
{
    return MQTT_Publish(topic, (const uint8_t *)str, strlen(str), 0);
}

/*===========================================================================
 * SUBSCRIBE
 *===========================================================================*/
uint8_t MQTT_Subscribe(const char *topic, uint8_t qos, uint32_t timeout_ms)
{
    uint16_t i = 0;
    uint8_t  len_bytes[4];
    uint8_t  len_size;
    uint32_t remaining;
    uint16_t pid;
    TickType_t start;

    if(topic == NULL) return MQTT_ERR_PARAM;
    if(!s_connected)  return MQTT_ERR_REFUSED;

    pid = s_packet_id++;
    if(s_packet_id == 0) s_packet_id = 1;

    remaining = 2 + 2 + strlen(topic) + 1;      /* PacketID + 主题 + QoS */

    /*--- 固定报头：SUBSCRIBE 的低4位必须是 0x02 ---*/
    s_tx_buf[i++] = MQTT_SUBSCRIBE | 0x02;
    len_size = mqtt_encode_len(len_bytes, remaining);
    memcpy(&s_tx_buf[i], len_bytes, len_size);
    i += len_size;

    /*--- 可变报头 ---*/
    s_tx_buf[i++] = (uint8_t)(pid >> 8);
    s_tx_buf[i++] = (uint8_t)(pid & 0xFF);

    /*--- Payload ---*/
    i += mqtt_write_string(&s_tx_buf[i], topic);
    s_tx_buf[i++] = qos & 0x03;

    mqtt_send(s_tx_buf, i);
    log_d("SUB [%s] qos=%d", topic, qos);

    /*--- 等 SUBACK: 90 03 <pid_h> <pid_l> <granted_qos> ---*/
    start = xTaskGetTickCount();
    while((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms))
    {
        if(Buffer_Length(&g_esp01_rx) >= 5)
        {
            uint8_t b0, b4;

            Buffer_Peek(&g_esp01_rx, 0, &b0);
            Buffer_Peek(&g_esp01_rx, 4, &b4);

            if(b0 == MQTT_SUBACK)
            {
                Buffer_Discard(&g_esp01_rx, 5);

                if(b4 == 0x80)
                {
                    log_e("SUBACK failure");
                    return MQTT_ERR_REFUSED;
                }
                log_i("subscribed [%s]", topic);
                return MQTT_OK;
            }
            /* 不是 SUBACK，可能是先到的 PUBLISH，交给 Poll 处理 */
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    log_w("SUBACK timeout");
    return MQTT_ERR_TIMEOUT;
}

///*===========================================================================
// * UNSUBSCRIBE
// *===========================================================================*/
//uint8_t MQTT_Unsubscribe(const char *topic, uint32_t timeout_ms)
//{
//    uint16_t i = 0;
//    uint8_t  len_bytes[4];
//    uint8_t  len_size;
//    uint32_t remaining;
//    uint16_t pid;
//    TickType_t start;

//    if(topic == NULL) return MQTT_ERR_PARAM;
//    if(!s_connected)  return MQTT_ERR_REFUSED;

//    pid = s_packet_id++;
//    if(s_packet_id == 0) s_packet_id = 1;

//    remaining = 2 + 2 + strlen(topic);

//    s_tx_buf[i++] = MQTT_UNSUBSCRIBE | 0x02;
//    len_size = mqtt_encode_len(len_bytes, remaining);
//    memcpy(&s_tx_buf[i], len_bytes, len_size);
//    i += len_size;

//    s_tx_buf[i++] = (uint8_t)(pid >> 8);
//    s_tx_buf[i++] = (uint8_t)(pid & 0xFF);
//    i += mqtt_write_string(&s_tx_buf[i], topic);

//    mqtt_send(s_tx_buf, i);

//    start = xTaskGetTickCount();
//    while((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms))
//    {
//        uint8_t b0;
//        if(Buffer_Length(&g_esp01_rx) >= 4)
//        {
//            Buffer_Peek(&g_esp01_rx, 0, &b0);
//            if(b0 == MQTT_UNSUBACK)
//            {
//                Buffer_Discard(&g_esp01_rx, 4);
//                return MQTT_OK;
//            }
//            break;
//        }
//        vTaskDelay(pdMS_TO_TICKS(20));
//    }
//    return MQTT_ERR_TIMEOUT;
//}

/*===========================================================================
 * PINGREQ / DISCONNECT
 *===========================================================================*/
uint8_t MQTT_Ping(void)
{
    uint8_t buf[2] = { MQTT_PINGREQ, 0x00 };

    if(!s_connected) return MQTT_ERR_REFUSED;

    mqtt_send(buf, 2);
    log_d("PING");
    return MQTT_OK;
}

void MQTT_Disconnect(void)
{
    uint8_t buf[2] = { MQTT_DISCONNECT, 0x00 };

    if(s_connected)
    {
        mqtt_send(buf, 2);
        s_connected = 0;
        log_i("MQTT disconnected");
    }
}

uint8_t MQTT_IsConnected(void)
{
    return s_connected;
}

/*===========================================================================
 * 接收解析
 *
 * 从环形缓冲区里取出一个完整报文。
 *===========================================================================*/
uint8_t MQTT_Poll(MQTT_Message_t *msg)
{
	/* 解析 PUBLISH报文 */
    uint8_t  fixed_hdr;
    uint32_t remaining;
    uint8_t  len_size;
    uint16_t total, topic_len;
    uint8_t  qos;
    uint16_t offset;
    uint8_t  hi, lo;
    uint16_t payload_len;
    uint16_t i;

    if(msg == NULL) return 0;
    if(Buffer_Length(&g_esp01_rx) < 2) return 0;

    /*--- 固定报头 ---*/
    Buffer_Peek(&g_esp01_rx, 0, &fixed_hdr);  //取出固定报头
 
    len_size = mqtt_decode_len(1, &remaining);  //取出剩余长度
    if(len_size == 0) return 0;                 

    total = 1 + len_size + remaining;  //解码剩余长度
    if(Buffer_Length(&g_esp01_rx) < total)
        return 0;                             //没接收全   

    /*--- 按报文类型分派 ---*/
    switch(fixed_hdr & 0xF0)
    {
    case MQTT_PUBLISH:
        qos    = (fixed_hdr >> 1) & 0x03;
        offset = 1 + len_size;

        /* 主题长度 */
        Buffer_Peek(&g_esp01_rx, offset,     &hi);
        Buffer_Peek(&g_esp01_rx, offset + 1, &lo);
        topic_len = ((uint16_t)hi << 8) | lo;

        if(topic_len >= MQTT_TOPIC_MAX_LEN)
        {
            log_w("topic too long: %d", topic_len);
            Buffer_Discard(&g_esp01_rx, total);
            return 0;
        }

        /* 主题内容 */
        offset += 2;
        for(i = 0; i < topic_len; i++)
        {
            uint8_t c;
            Buffer_Peek(&g_esp01_rx, offset + i, &c);
            msg->topic[i] = (char)c;
        }
        msg->topic[topic_len] = '\0';
        offset += topic_len;

        /* QoS>0 时跳过报文标识符 */
        if(qos > 0) offset += 2;

        /* Payload */
        payload_len = total - offset;
        if(payload_len >= MQTT_PAYLOAD_MAX)
        {
            log_w("payload truncated: %d", payload_len);
            payload_len = MQTT_PAYLOAD_MAX - 1;
        }

        for(i = 0; i < payload_len; i++)
            Buffer_Peek(&g_esp01_rx, offset + i, &msg->payload[i]);

        msg->payload[payload_len] = '\0';
        msg->payload_len = payload_len;

        Buffer_Discard(&g_esp01_rx, total);
        log_d("RECV [%s] %d bytes", msg->topic, payload_len);
        return 1;

    case MQTT_PINGRESP:
        Buffer_Discard(&g_esp01_rx, total);
        log_d("PINGRESP");
        return 0;

    case MQTT_PUBACK:
    case MQTT_SUBACK:
    case MQTT_UNSUBACK:
        Buffer_Discard(&g_esp01_rx, total);
        return 0;

    default:
        /* 未知报文，丢弃避免堵塞 */
        log_w("unknown packet 0x%02X", fixed_hdr);
        Buffer_Discard(&g_esp01_rx, total);
        return 0;
    }
}

/*===========================================================================
 * 心跳维护
 * 距上次发送超过 keepalive 的一半就补一个 PINGREQ
 *===========================================================================*/
void MQTT_KeepAliveTick(void)
{
    if(!s_connected) return;

    if((xTaskGetTickCount() - s_last_send_tick) >
       pdMS_TO_TICKS(MQTT_KEEPALIVE_SEC * 500))
    {
        MQTT_Ping();
    }
}