#include "stm32f4xx.h"
#include "mydelay.h"
#include "mydht11.h"
#include "FreeRTOS.h"
#include "task.h"
//#include "elog.h" 
#include "stdio.h"


#define DHT11_PORT      GPIOE
#define DHT11_PIN       GPIO_Pin_9
#define DHT11_READ()    GPIO_ReadInputDataBit(DHT11_PORT, DHT11_PIN)
#define DHT11_HIGH()    (DHT11_PORT->BSRRL = DHT11_PIN)
#define DHT11_LOW()     (DHT11_PORT->BSRRH = DHT11_PIN)

static void DHT11_Mode_Out(void)
{
    GPIO_InitTypeDef s;
    s.GPIO_Mode  = GPIO_Mode_OUT;
    s.GPIO_OType = GPIO_OType_PP;
    s.GPIO_PuPd  = GPIO_PuPd_UP;
    s.GPIO_Pin   = DHT11_PIN;
    s.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(DHT11_PORT, &s);
}

static void DHT11_Mode_In(void)
{
    GPIO_InitTypeDef s;
    s.GPIO_Mode = GPIO_Mode_IN;
    s.GPIO_PuPd = GPIO_PuPd_UP;
    s.GPIO_Pin  = DHT11_PIN;
    GPIO_Init(DHT11_PORT, &s);
}

void DHT11_Init(void)
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
    DHT11_Mode_Out();
    DHT11_HIGH();
}

// 电平等待，返回 0=成功 1=超时 
static uint8_t DHT11_WaitLevel(uint8_t level, uint32_t timeout_us)
{
    while(DHT11_READ() == level)
    {
        if(timeout_us-- == 0) 
			return 1;
        MyDelay_us(1);
    }
    return 0;
}

/*
 * 返回
 *	0=成功
 *  1=无响应  
 *	2=应答超时  3=数据超时  4=校验失败
 */
uint8_t DHT11_Read(DHT11_Data_t *out)
{
    uint8_t buf[5] = {0};
    uint8_t i, j, ret = 0;
	
	uint8_t t = 0;

    //起始信号：拉低 20ms
    DHT11_Mode_Out();
    DHT11_LOW();
    vTaskDelay(pdMS_TO_TICKS(30));
	
	DHT11_HIGH();
	
    MyDelay_us(30);
    DHT11_Mode_In();
	

    //进入临界区
    taskENTER_CRITICAL();

    do {
//        while(DHT11_READ() == 1)
//		{
//			MyDelay_us(1);
//			t++;
//			if(t >= 50)
//			{
//				ret = 1;break;
//			}
//		}
//		t = 0;
//		while(DHT11_READ() == 0)
//		{
//			MyDelay_us(1);
//			t++;	
//			if(t > 90)
//			{
//				ret = 2;break;
//			}
//		}		
//		
//	//应答高电平	
//		t = 0;
//		while(DHT11_READ() == 1)
//		{
//			MyDelay_us(1);
//			t++;
//			if(t > 95)
//			{
//				ret = 3; break;
//			}
//		}
        if(DHT11_WaitLevel(1, 100)){
			ret = 1; break;
		}
        /* 响应低电平 80us */
        if(DHT11_WaitLevel(0, 100)){
			ret = 2; break;
		}
        /* 响应高电平 80us */
        if(DHT11_WaitLevel(1, 100)){
			ret = 2; break;
		}
        /* --- 读 40 bit --- */
        for(i = 0; i < 5; i++)
        {
            for(j = 0; j < 8; j++)
            {
                /* 每位以 50us 低电平开始 */
                //if(DHT11_WaitLevel(0, 100)) { ret = 3; goto exit; }
				while(DHT11_READ() == 0);

                /* 高电平 28us=0, 70us=1，延时 40us 后采样 */
                MyDelay_us(45);

                buf[i] <<= 1;
                if(DHT11_READ()) buf[i] |= 0x01;

                /* 等本位高电平结束 */
                //if(DHT11_WaitLevel(1, 100)) { ret = 3; goto exit; }
				while(DHT11_READ() == 1);
            }
        }
    } while( 0);

exit:
    taskEXIT_CRITICAL();

    if(ret != 0) return ret;

    /* 校验和 */
    if((uint8_t)(buf[0] + buf[1] + buf[2] + buf[3]) != buf[4]) return 4;

    out->humi_int  = buf[0];
    out->humi_dec  = buf[1];
    out->temp_int  = buf[2];
    out->temp_dec  = buf[3];
    return 0;
}



///*
//	PE9 ----- data
//*/
//void PE9_OUT(void)
//{
//	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE,ENABLE);
//	
//	GPIO_InitTypeDef Struct;
//    Struct.GPIO_Mode = GPIO_Mode_OUT ;
//    Struct.GPIO_OType = GPIO_OType_PP;  
//    Struct.GPIO_Pin = GPIO_Pin_9;
//    Struct.GPIO_Speed = GPIO_Speed_100MHz;
//    GPIO_Init(GPIOE,&Struct);
//}

//void PE9_IN(void)
//{
//	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE,ENABLE);
//    
//    GPIO_InitTypeDef Struct;
//    Struct.GPIO_Mode = GPIO_Mode_IN ; 
//    Struct.GPIO_PuPd = GPIO_PuPd_UP;
//    Struct.GPIO_Pin = GPIO_Pin_9;
//    GPIO_Init(GPIOE,&Struct);
//}
///* 写数据线 */
//void SDA_Write(uint8_t data)
//{
//	//非0 即 1
//	GPIO_WriteBit(GPIOE,GPIO_Pin_9,(BitAction)data);
//}

///* 封装时序 */
//uint8_t DHT11_Start(void)
//{
//	
//	uint32_t i = 0;
//	//拉低总线
//	PE9_OUT();
//	
//	SDA_Write(0);
//	MyDelay_ms(30);	
//	//释放总线
//	SDA_Write(1);	
//	PE9_IN();  //切换为输入
//	while(GPIO_ReadInputDataBit(GPIOE,GPIO_Pin_9) == 1)
//	{
//		MyDelay_us(1);
//		i++;
//		if(i >= 50)
//		{
//			printf("ERROR\n");
//			return -1;
//		}
//	}	
//	//应答低电平
//	i = 0;
//	while(GPIO_ReadInputDataBit(GPIOE,GPIO_Pin_9) == 0)
//	{
//		MyDelay_us(1);
//		i++;	
//		if(i > 90){
//			printf("ACK1_90\n");
//			return -1;
//		}		
//	}		
//	//应答高电平	
//	i = 0;
//		while(GPIO_ReadInputDataBit(GPIOE,GPIO_Pin_9) == 1)
//	{
//		MyDelay_us(1);
//		i++;
//		if(i > 95){
//			printf("ACK2_95\n");
//			return -1;
//		}
//	}
//	return 0;
//}

////接收数据
//void DHT11_Recv_Data(void)
//{
//	uint8_t arr[5] = {0}; //用来存放40bit
//	for(uint8_t i = 0; i < 5; i++)
//	{
//		for(uint8_t j = 0; j < 8; j++)
//		{
//			while(GPIO_ReadInputDataBit(GPIOE,GPIO_Pin_9) == 0);
//			MyDelay_us(50);
//			if(GPIO_ReadInputDataBit(GPIOE,GPIO_Pin_9) == 1)
//			{
//				arr[i] |= (1 << (7 - j)); 
//			}
//			while(GPIO_ReadInputDataBit(GPIOE,GPIO_Pin_9) == 1);
//		}
//	}	
//	printf("%d:%d\n",arr[2],arr[3]); //温度	
//}

////结束通信
//void DHT11_END(void)
//{
//	SDA_Write(1);
//}


