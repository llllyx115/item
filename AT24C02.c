#include "stm32f4xx.h"                  // Device header
#include "IIC.h"
#include "mydelay.h"
#include "stdio.h"

//写一个字节到at24c02
uint8_t AT24C02_WriteByte(uint8_t add,uint8_t data)
{
	IIC_Start();
	IIC_SendByte(0xA0);//写
	if(IIC_WaitAck())
		goto err;
	
	IIC_SendByte(add);
	if(IIC_WaitAck())
		goto err;
	
	IIC_SendByte(data);
	if(IIC_WaitAck())
		goto err;

	IIC_Stop();
	MyDelay_ms(10);//EEPROM写时间
	return 0;
	err:
	IIC_Stop();
	printf("[IIC] err\n");
	return 1;
}
	
//指定一个地址读
uint8_t AT24C02_ReadByte(uint8_t add)
{
	uint8_t data = 0;
	IIC_Start();
	
	IIC_SendByte(0xA0);
	if(IIC_WaitAck()) //检查ACK
		goto err;
	
	IIC_SendByte(add);
	if(IIC_WaitAck())
		goto err;
	
	IIC_Start();
	
	IIC_SendByte(0xA1);
	if(IIC_WaitAck())
		goto err;
	
	data = IIC_ReadByte(1);

	IIC_Stop();
	return data;

	err:
	IIC_Stop();
	printf("[IIC] read error\n");
	return -1;
}