#include "stm32f4xx.h"                  // Device header
#include "mydelay.h"
#include "IIC.h"
/*
PB9 --- SDA -输入输出转换
PB8 --- SCL -输出模式
*/
////IIC基本时序
//void SDA_HIGH(void); //释放SDA
//void SDA_LOW(void); //拉低SDA 
//void SCL_HIGH(void); //释放SCL
//void SCL_LOW(void); //拉低SCL

void My_IIC_GpioInit()
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB,ENABLE);

	//开漏+上拉
	GPIO_InitTypeDef S1;
    S1.GPIO_Mode = GPIO_Mode_OUT;
    S1.GPIO_OType = GPIO_OType_OD;  //开漏输出
    S1.GPIO_PuPd = GPIO_PuPd_UP;
    S1.GPIO_Speed = GPIO_Speed_100MHz;
    
    //SCL PB8
    S1.GPIO_Pin = GPIO_Pin_8;
    GPIO_Init(GPIOB,&S1);
    
    S1.GPIO_Pin = GPIO_Pin_9;
    GPIO_Init(GPIOB,&S1);
    
    //释放总线
    GPIO_SetBits(GPIOB,GPIO_Pin_8|GPIO_Pin_9);
	

}

void SDA_OUT(void)
{
	GPIO_InitTypeDef S;
    
    S.GPIO_Pin  = GPIO_Pin_9;
    S.GPIO_Mode = GPIO_Mode_OUT;
    S.GPIO_OType = GPIO_OType_OD;
    S.GPIO_PuPd = GPIO_PuPd_UP;
    S.GPIO_Speed = GPIO_Speed_100MHz;
    
    GPIO_Init(GPIOB,&S);
}

void SDA_IN(void)
{
	GPIO_InitTypeDef S;

    S.GPIO_Pin = GPIO_Pin_9;
    S.GPIO_Mode = GPIO_Mode_IN;
    S.GPIO_PuPd = GPIO_PuPd_UP;

    GPIO_Init(GPIOB,&S);
}

uint8_t SDARead(void)//读取SDA电平*/
{
	return GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_9);
}

void IIC_Start(void)
{
	SDA_OUT();
	SCL_HIGH();
	SDA_HIGH();
	MyDelay_us(5);
	
	SDA_LOW();
	//-----通信开始-----//
	MyDelay_us(5);//获取完整方波信号
	SCL_LOW();
	
}

//发送一个字节数据
//SCL拉低的时候是主机发送数据 拉高是从机（接收方）接收数据
void IIC_SendByte(uint8_t data)
{
	SDA_OUT();//数据线输出 发送数据
	
	for(uint8_t i = 0;i < 8;i++)
	{
		SCL_LOW();
		
		if(data & 0x80){
		SDA_HIGH();
		}else{
			SDA_LOW();
		}
		data <<= 1;//每次都和最高位与
		MyDelay_us(5);
		
		SCL_HIGH();//从机开始读
		MyDelay_us(5);
		
	}
	SDA_HIGH();//拉高数据线 如果从机正常会拉低
	SCL_LOW();
}

//等待ACK
//SCL的高低只有主机能控制
uint8_t IIC_WaitAck(void)
{
	SDA_IN();//主机准备接收数据
	MyDelay_us(5);
	
	SCL_HIGH();//主机接收数据开始
	if(SDARead() == 0)
	{
		SCL_LOW();//主机继续发送数据
		return 0;//收到ACK
	}
	else{
	return 1;//未收到ACK
	}
	
}

void IIC_Stop(void)//sda拉高的时候 scl是高
{
	SDA_OUT();
	
	SCL_LOW();
	SDA_LOW();
	
	MyDelay_us(5);
	
	SCL_HIGH();
	MyDelay_us(5);
	
	SDA_HIGH();
	MyDelay_us(5);
}

//发送ACK（0）
void IIC_Ack(void)
{
	SDA_OUT();
	
	SDA_LOW();
	MyDelay_us(5);
	SCL_HIGH();
	
	MyDelay_us(5);
	
	SCL_LOW();//从机继续发
}

void IIC_NAck(void)
{
	SDA_OUT();
	SDA_HIGH();
	MyDelay_us(5);
	
	SCL_HIGH();
	MyDelay_us(5);
	
	//通信要结束
}

//接收一个字节
uint8_t IIC_ReadByte(uint8_t ack)
{
	uint8_t data = 0;
	
	SDA_IN();
	for(uint8_t i = 0;i < 8;i ++)
	{
		MyDelay_us(5);
		
		SCL_HIGH();
		data <<= 1;
		if(SDARead() == 1)
		{
			data++;
		}
		SCL_LOW();
	}
	if(ack == 0)
	{
		IIC_Ack();
	}
	else{IIC_NAck();}
	
	return data;
}
