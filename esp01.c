#define LOG_TAG "esp01"

#include "stm32f4xx.h"                  // Device header
#include "elog.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "buffer.h"
#include "esp01.h"

extern uint8_t s_esp01_rx_mem[ESP01_RX_BUF_SIZE];
extern BufferTypeDef g_esp01_rx;    /* 环形缓冲区 */
extern SemaphoreHandle_t g_esp01_rx_sem; /* 一帧接收完成通知 */

/* 标记 WiFi + TCP 是否就绪 */
uint8_t s_esp01_ready = 0;


/* 命令字符串 */
char * at         = "AT\r\n";
char * at_cwmode  = "AT+CWMODE=1\r\n";    // 设置为STA模式
char * at_rst     = "AT+RST\r\n"; 
char * at_cwjap   = "AT+CWJAP=\"lingjun\",\"LJ0147258369\"\r\n";
char * at_cipmux  = "AT+CIPMUX=0\r\n";    // 开启单连接
char * at_cipstart= "AT+CIPSTART=\"TCP\",\"192.168.2.53\",1883\r\n";
char * at_cipmode = "AT+CIPMODE=1\r\n";   // 开启透传模式
char * at_cipsend = "AT+CIPSEND\r\n";     // 进入透传发送状态

/*
	通过USART2 和ESP01交互
	
	Tx --- PA2
	Rx --- PA3
*/
void Esp01_Usart2Init(void)
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2,ENABLE);
    
    GPIO_InitTypeDef Struct;
    Struct.GPIO_Mode = GPIO_Mode_AF;
    Struct.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3;
    Struct.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(GPIOA,&Struct);
    
    GPIO_PinAFConfig(GPIOA,GPIO_PinSource2,GPIO_AF_USART2);
    GPIO_PinAFConfig(GPIOA,GPIO_PinSource3,GPIO_AF_USART2);
    
    USART_InitTypeDef Struct2;
    Struct2.USART_BaudRate = 115200;  //波特率
    Struct2.USART_HardwareFlowControl = USART_HardwareFlowControl_None; //硬件流控  不使用硬件信号控制数据传输
    Struct2.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;   //同时支持读写
    Struct2.USART_Parity = USART_Parity_No; //校验位
    Struct2.USART_StopBits = USART_StopBits_1;  //一个停止位
    Struct2.USART_WordLength = USART_WordLength_8b; //有效数据长度
    USART_Init(USART2,&Struct2);
    
	USART_ITConfig(USART2,USART_IT_RXNE,ENABLE); //打开接收中断
	USART_ITConfig(USART2,USART_IT_IDLE,ENABLE); //打开空闲中断
	
	NVIC_InitTypeDef Struct3;
    Struct3.NVIC_IRQChannel = USART2_IRQn;
    Struct3.NVIC_IRQChannelCmd = ENABLE;
    Struct3.NVIC_IRQChannelPreemptionPriority = 6; //优先级 > SRF04(7)
    Struct3.NVIC_IRQChannelSubPriority = 0;
    NVIC_Init(&Struct3);
		
	USART_Cmd(USART2,ENABLE);
	log_d("Esp01_Usart2Init OK");	
}

/* 
	串口2的字符串发送函数
*/
void Usart2_SendString(uint8_t *str)
{
	uint8_t n;
	for(n = 0; str[n] != '\0'; n++)
	{
		USART_SendData(USART2,str[n]);
		while(USART_GetFlagStatus(USART2,USART_FLAG_TXE) == 0);
	}
}

/*
	esp01 发送命令函数
*/
uint8_t ESP01_SendCmd(char *cmd,char *ack,uint16_t waitetime)
{
	//自己实现
	TickType_t start = xTaskGetTickCount(); //获取
	 	
	Buffer_Reset(&g_esp01_rx); /* 清掉旧数据 */
	xQueueReset((QueueHandle_t)g_esp01_rx_sem);  /* 清零信号量 */
	
	Usart2_SendString((uint8_t *)cmd); /* 发送命令 */
	log_d("---> %s",cmd);
	
	while((xTaskGetTickCount() - start) < pdMS_TO_TICKS(waitetime))
    {
        /* 50ms查一次 */
        xSemaphoreTake(g_esp01_rx_sem, pdMS_TO_TICKS(50));

        if(Buffer_Find(&g_esp01_rx, ack) >= 0)
        {
            log_d("<-- ack [%s] OK", ack);
            return 0;
        }
        if(Buffer_Find(&g_esp01_rx, "ERROR") >= 0)
        {
            log_e("<-- cmd ERROR");
            return 1;
        }
        if(Buffer_Find(&g_esp01_rx, "FAIL") >= 0)
        {
            log_e("<-- cmd FAIL");
            return 1;
        }
    }

    log_w("<- timeout waiting [%s]", ack);
    return 2;
	
	
}

void Esp01_Init(void)
{
	/* 缓冲区和信号量必须在中断之前创建 */
	Buffer_Init(&g_esp01_rx,s_esp01_rx_mem,ESP01_RX_BUF_SIZE);
	
	g_esp01_rx_sem = xSemaphoreCreateBinary();
	if(g_esp01_rx_sem == NULL)
    {
        log_e("esp01 rx sem create failed");
        return;
    }
	
	Esp01_Usart2Init();  
	
	/* AT 指令初始化序列*/
	//一条条发送指令
	
	if(ESP01_SendCmd(at,"OK",500) != 0)
	{
		log_e("AT ERROR");
		return;
	}
	if(ESP01_SendCmd(at_cwmode, "OK", 2000) != 0)
	{
		log_e("CWMODE failed");
		return;
	}
	
	log_d("connect wifi........wait.......");
    if(ESP01_SendCmd(at_cwjap, "WIFI GOT IP", 20000) != 0)   /* 连 WiFi 时间长一点*/
	{
		log_e("WiFi connect failed");
        return;
	}
	log_d("connect wifi SUCCESS");
	
    if(ESP01_SendCmd(at_cipmux, "OK", 2000) != 0)
    {
        log_e("CIPMUX failed");
        return;
    }
	
	/* 连接服务器 */
	log_d("connect Server........wait.......");
	if(ESP01_SendCmd(at_cipstart, "CONNECT", 10000) != 0)  /* 时间稍微长一点*/
    {
        log_e("TCP connect failed");
        return;
    }
	log_d("connect Server SUCCESS");
	
    /* 开启透传模式 */
    if(ESP01_SendCmd(at_cipmode, "OK", 2000) != 0)
    {
        log_e("CIPMODE failed");
        return;
    }
	
	/* 进入透传发送状态，应答是 ">" */
	/* 要退出透传模式， 发送+++三个加号，但是前后要有1秒以上的延时*/
	if(ESP01_SendCmd(at_cipsend, ">", 2000) != 0)
    {
        log_e("CIPSEND failed");
        return;
    }
	
	s_esp01_ready = 1; //和服务器建立TCP连接成功
	/* 问候一下服务器 */
	//Usart2_SendString((uint8_t *)"Hello I am Client");
	
    log_i("ESP01 init done");
		
}

/* 服务器下发的消息 通过串口中断接收*/


uint8_t Esp01_IsReady(void)
{ 
	return s_esp01_ready; 

}


/* 按长度发送原始字节*/
void Usart2_SendBuf(uint8_t *buf, uint16_t len)
{
    for(uint16_t i = 0; i < len; i++)
    {
        USART_SendData(USART2, buf[i]);
        while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
    }
}


uint8_t ESP01_WaitAck(char *ack, uint16_t waittime)
{
    TickType_t start = xTaskGetTickCount();

    while((xTaskGetTickCount() - start) < pdMS_TO_TICKS(waittime))
    {
        xSemaphoreTake(g_esp01_rx_sem, pdMS_TO_TICKS(20));

        if(Buffer_Find(&g_esp01_rx, ack) >= 0) return 0;
        if(Buffer_Find(&g_esp01_rx, "ERROR") >= 0) return 1;
    }
    return 2;
}
