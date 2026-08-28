#ifndef __MYMQTT_H__
#define __MYMQTT_H__

#include "stm32f4xx.h"

/*===========================================================================
 * MQTT 3.1.1 
 *===========================================================================*/

/*--- 报文类型 (固定报头高4位) ---*/
#define MQTT_CONNECT        0x10
#define MQTT_CONNACK        0x20
#define MQTT_PUBLISH        0x30
#define MQTT_PUBACK         0x40
#define MQTT_PUBREC         0x50
#define MQTT_PUBREL         0x60
#define MQTT_PUBCOMP        0x70
#define MQTT_SUBSCRIBE      0x80
#define MQTT_SUBACK         0x90
#define MQTT_UNSUBSCRIBE    0xA0
#define MQTT_UNSUBACK       0xB0
#define MQTT_PINGREQ        0xC0
#define MQTT_PINGRESP       0xD0
#define MQTT_DISCONNECT     0xE0

/*--- CONNACK 返回码 ---*/
#define MQTT_CONNACK_OK             0x00
#define MQTT_CONNACK_BAD_PROTOCOL   0x01
#define MQTT_CONNACK_BAD_CLIENTID   0x02
#define MQTT_CONNACK_UNAVAILABLE    0x03
#define MQTT_CONNACK_BAD_AUTH       0x04
#define MQTT_CONNACK_NOT_AUTHORIZED 0x05

/*--- 函数返回码 ---*/
#define MQTT_OK             0
#define MQTT_ERR_TIMEOUT    1
#define MQTT_ERR_REFUSED    2
#define MQTT_ERR_PARAM      3
#define MQTT_ERR_BUFFER     4

/*--- 配置 ---*/
#define MQTT_TX_BUF_SIZE    512     
#define MQTT_TOPIC_MAX_LEN  64
#define MQTT_PAYLOAD_MAX    256
#define MQTT_KEEPALIVE_SEC  60

/*--- 收到的消息 ---*/
typedef struct {
    char     topic[MQTT_TOPIC_MAX_LEN];
    uint8_t  payload[MQTT_PAYLOAD_MAX];
    uint16_t payload_len;
} MQTT_Message_t;

/*--- 连接参数 ---*/
typedef struct {
    const char *client_id;
    const char *username;       /* 不需要认证时填 NULL */
    const char *password;       /* 不需要认证时填 NULL */
    uint16_t    keepalive;      /* 秒，0 表示不启用心跳 */
} MQTT_ConnParam_t;

/*--- 接口 ---*/
uint8_t MQTT_Connect(const MQTT_ConnParam_t *param, uint32_t timeout_ms);
uint8_t MQTT_Publish(const char *topic, const uint8_t *payload, uint16_t len, uint8_t qos);
uint8_t MQTT_PublishStr(const char *topic, const char *str);
uint8_t MQTT_Subscribe(const char *topic, uint8_t qos, uint32_t timeout_ms);
uint8_t MQTT_Unsubscribe(const char *topic, uint32_t timeout_ms);
uint8_t MQTT_Ping(void);
void    MQTT_Disconnect(void);

/* 从环形缓冲区解析一个报文。
 * 返回 1 表示解析出一条 PUBLISH 消息（结果写入 msg），0 表示暂无完整消息 */
uint8_t MQTT_Poll(MQTT_Message_t *msg);

/* 心跳维护，在任务里周期调用 */
void    MQTT_KeepAliveTick(void);
uint8_t MQTT_IsConnected(void);

#endif