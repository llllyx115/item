#ifndef __SCCB_H__
#define __SCCB_H__

#include "stm32f4xx.h"
#include <stdint.h>

/*===========================================================================
 * SCCB 引脚（软件模拟 I2C）
 *   PD6 = SCL
 *   PD7 = SDA
 *
 * SCCB 不是标准 I2C，读操作要求 "写地址 -> Stop -> Start -> 读"，
 * 硬件 I2C 外设发不出中间那个 Stop，所以只能软件模拟。
 *===========================================================================*/

/* SDA 方向切换：直接改 MODER，比 GPIO_Init 快得多 */
#define SCCB_SDA_IN()   do { GPIOD->MODER &= ~(3u << (7*2)); } while(0)
#define SCCB_SDA_OUT()  do { GPIOD->MODER &= ~(3u << (7*2)); \
                             GPIOD->MODER |=  (1u << (7*2)); } while(0)

#define SCCB_SCL(x)     GPIO_WriteBit(GPIOD, GPIO_Pin_6, (BitAction)(x))
#define SCCB_SDA(x)     GPIO_WriteBit(GPIOD, GPIO_Pin_7, (BitAction)(x))
#define SCCB_READ_SDA   GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_7)

/* OV2640 SCCB 写地址 */
#define SCCB_ID         0x60

void    SCCB_Init(void);
void    SCCB_Start(void);
void    SCCB_Stop(void);
void    SCCB_No_Ack(void);
uint8_t SCCB_WR_Byte(uint8_t dat);
uint8_t SCCB_RD_Byte(void);
uint8_t SCCB_WR_Reg(uint8_t reg, uint8_t data);
uint8_t SCCB_RD_Reg(uint8_t reg);

#endif /* __SCCB_H__ */