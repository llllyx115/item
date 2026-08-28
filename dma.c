#include "stm32f4xx.h"                  // Device header
#include "string.h"

/*从内存 ->外设串口*/

const uint8_t DMA_SendBuf[] = "As flowers fall and fly across the skies,\
    Who rues the red that fades,the scent that dies?Softly the gossamer floats over bowers green;\
    Gently the willow fluff wafts to broidered screen.\
    In my chamber i am grieved to see spring depart.\
    Where can I pour out my sorrow-laden heart?\n";

void MyDma1Init(void)
{
	//1.开时钟(USART1 GPIOA)
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE);
	RCC_APB2PeriphClockCmd (RCC_APB2Periph_USART1,ENABLE);
	
	//2.GPIO配置
	//单独配置PA9 Tx 复用推挽输出
	GPIO_InitTypeDef S1;
	
	S1.GPIO_Mode = GPIO_Mode_AF; //复用
	S1.GPIO_OType = GPIO_OType_PP;  //推挽输出
	S1.GPIO_Pin = GPIO_Pin_9;//PA9
	S1.GPIO_Speed = GPIO_Speed_50MHz;//速度
	GPIO_Init(GPIOA,&S1);
	
	//3.单独配置PA10 Rx 复用输入
	S1.GPIO_Mode = GPIO_Mode_AF;//复用输入
	S1.GPIO_Pin = GPIO_Pin_10;//PA10
	S1.GPIO_PuPd = GPIO_PuPd_UP;//给个上拉
  GPIO_Init(GPIOA,&S1);
	
	//4.指定复用外设
	GPIO_PinAFConfig(GPIOA,GPIO_PinSource9,GPIO_AF_USART1);//将GPIOA的第九个引脚复用到USART1上
	GPIO_PinAFConfig(GPIOA,GPIO_PinSource10,GPIO_AF_USART1);
	
	//5.初始化串口USART1 配置参数
	
	
	USART_InitTypeDef S2;
	
  S2.USART_BaudRate = 115200;//波特率
  S2.USART_HardwareFlowControl = USART_HardwareFlowControl_None;//(这里不需要) 硬件流（字节）控制
  S2.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;//收发模式 
  S2.USART_Parity = USART_Parity_No;//无校验
  S2.USART_StopBits = USART_StopBits_1;//停止位
  S2.USART_WordLength = USART_WordLength_8b;//有效数据长度	
	
	USART_Init(USART1,&S2); 
	
	//使能串口1的DMA请求
	USART_DMACmd(USART1,USART_DMAReq_Tx,ENABLE);
	
	//6.使能串口
	USART_Cmd(USART1,ENABLE); 
	
}

void DMA2_Stream1_Chan4_Init(void)
{
	//打开时钟
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2,ENABLE);
	
	//关闭流
	DMA_Cmd(DMA2_Stream7,DISABLE);
	while(DMA_GetCmdStatus(DMA2_Stream7) != DISABLE);
	
	//DMA的具体配置
	DMA_InitTypeDef S;
	S.DMA_BufferSize = strlen((char*)DMA_SendBuf);//传输的总数据长度
	S.DMA_Channel = DMA_Channel_4;
	S.DMA_DIR = DMA_DIR_MemoryToPeripheral;//传输方向 内存到外设
 	S.DMA_FIFOMode = DMA_FIFOMode_Enable;//是否启用FIFO
	S.DMA_FIFOThreshold = DMA_FIFOThreshold_HalfFull;//FIFO的阈值
	S.DMA_Memory0BaseAddr = (uint32_t)DMA_SendBuf;//内存的起始地址
	S.DMA_MemoryBurst = DMA_MemoryBurst_INC8;//内存的突发长度
	S.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;//存储器数据宽度
	S.DMA_MemoryInc = DMA_MemoryInc_Enable;//内存自增
	S.DMA_Mode = DMA_Mode_Normal;//单次传输
	S.DMA_PeripheralBaseAddr = (uint32_t)&USART1->DR;//串口的数据寄存器
	S.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;//外设不突发
	S.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;//外设数据宽度
	S.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	S.DMA_Priority = DMA_Priority_VeryHigh;//优先级
	DMA_Init(DMA2_Stream7,&S);
}


