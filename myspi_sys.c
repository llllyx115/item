#include "stm32f4xx.h"                  // Device header
#include "myspi_sys.h"

/* CS 用软件  */
#define SPI_CS_H()      (SPI_GPIO_PORT->BSRRL = SPI_PIN_CS)
#define SPI_CS_L()      (SPI_GPIO_PORT->BSRRH = SPI_PIN_CS)

void MySPI_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    SPI_InitTypeDef  SPI_InitStruct;

    RCC_AHB1PeriphClockCmd(SPI_GPIO_CLK, ENABLE);
    RCC_APB1PeriphClockCmd(SPIx_CLK, ENABLE);

    /* CS: 推挽输出，软件控制 */
    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStruct.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_InitStruct.GPIO_Pin   = SPI_PIN_CS;
    GPIO_Init(SPI_GPIO_PORT, &GPIO_InitStruct);

    /* SCK / MISO / MOSI: 复用功能 */
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStruct.GPIO_Pin  = SPI_PIN_SCK | SPI_PIN_MISO | SPI_PIN_MOSI;
    GPIO_Init(SPI_GPIO_PORT, &GPIO_InitStruct);

    GPIO_PinAFConfig(SPI_GPIO_PORT, SPI_SRC_SCK,  SPI_AF);
    GPIO_PinAFConfig(SPI_GPIO_PORT, SPI_SRC_MISO, SPI_AF);
    GPIO_PinAFConfig(SPI_GPIO_PORT, SPI_SRC_MOSI, SPI_AF);

    /* SPI 参数: 模式0 (CPOL=0, CPHA=1Edge), MSB先行, 8位 */
    SPI_InitStruct.SPI_Direction         = SPI_Direction_2Lines_FullDuplex;  //两线全双工
    SPI_InitStruct.SPI_Mode              = SPI_Mode_Master; //主从-MCU做为主机
    SPI_InitStruct.SPI_DataSize          = SPI_DataSize_8b;
    SPI_InitStruct.SPI_CPOL              = SPI_CPOL_Low; //模式0 空闲SCL为低
    SPI_InitStruct.SPI_CPHA              = SPI_CPHA_1Edge;  //第一个边沿采样
    SPI_InitStruct.SPI_NSS               = SPI_NSS_Soft;  //片选线由软件来实现
    /* SPI2 挂在 APB1 (42MHz)，2分频 = 21MHz，W25Q64 最高支持 80MHz */
    SPI_InitStruct.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;  //SCL的频率，这里2分频
    SPI_InitStruct.SPI_FirstBit          = SPI_FirstBit_MSB; //最高有效位先发
    SPI_InitStruct.SPI_CRCPolynomial     = 7;  //CRC校验（7默认，不起作用）
    SPI_Init(SPIx, &SPI_InitStruct);

    /* 关键: 软件NSS模式下必须把 SSI 置1，否则主模式会触发 MODF 错误 */
    SPI_NSSInternalSoftwareConfig(SPIx, SPI_NSSInternalSoft_Set);

    SPI_Cmd(SPIx, ENABLE);

    SPI_CS_H();
}

void MySPI_Start(void)
{
    SPI_CS_L();
}

void MySPI_Stop(void)
{
    SPI_CS_H();
}

uint8_t MySPI_SwapByte(uint8_t ByteSend)
{
    /* 等待发送缓冲区空 */
    while((SPIx->SR & SPI_I2S_FLAG_TXE) == 0);
    SPIx->DR = ByteSend;

    /* 等待接收缓冲区非空 */
    while((SPIx->SR & SPI_I2S_FLAG_RXNE) == 0);
    return (uint8_t)SPIx->DR;
}

/* 只发送，丢弃读回的数据 */
void MySPI_SendBuf(const uint8_t *buf, uint32_t len)
{
    while(len--)
    {
        while((SPIx->SR & SPI_I2S_FLAG_TXE) == 0);
        SPIx->DR = *buf++;
        while((SPIx->SR & SPI_I2S_FLAG_RXNE) == 0);
        (void)SPIx->DR;   
    }
}

/* 只接收，0xFF */
void MySPI_RecvBuf(uint8_t *buf, uint32_t len)
{
    while(len--)
    {
        while((SPIx->SR & SPI_I2S_FLAG_TXE) == 0);
        SPIx->DR = 0xFF;
        while((SPIx->SR & SPI_I2S_FLAG_RXNE) == 0);
        *buf++ = (uint8_t)SPIx->DR;
    }
}