#include "stm32f4xx.h"                  // Device header
#include "IIC.h"

extern uint8_t oled_arr[8][128];

/*
   命令发送函数
   地址：0x78
	 0x00
*/
uint8_t IIC_SendCommondToOled(uint8_t Commond)
{
	IIC_Start();
	IIC_SendByte(0x78);
	if(IIC_WaitAck())
	goto err;
	
	IIC_SendByte(0x00);//表示这是命令
	if(IIC_WaitAck())
	goto err;
	
	IIC_SendByte(Commond);
	if(IIC_WaitAck())
	goto err;
	
	IIC_Stop();
	return 0;
	
	err:
		IIC_Stop();
		return 1;
}

uint8_t IIC_SendDataToOled(uint8_t Commond)
{
	IIC_Start();
	IIC_SendByte(0x78);
	if(IIC_WaitAck())
	goto err;
	
	IIC_SendByte(0x40);//表示这是命令
	if(IIC_WaitAck())
	goto err;
	
	IIC_SendByte(Commond);
	if(IIC_WaitAck())
	goto err;
	
	IIC_Stop();
	return 0;
	
	err:
		IIC_Stop();
		return 1;
}

void OLED_Init(void)
{
	IIC_SendCommondToOled(0xAE); // 关闭显示
	IIC_SendCommondToOled(0xD5);
	IIC_SendCommondToOled(0x80);
	IIC_SendCommondToOled(0xA8);
	IIC_SendCommondToOled(0x3F);
	IIC_SendCommondToOled(0xD3);
	IIC_SendCommondToOled(0x00);
	IIC_SendCommondToOled(0x40);
	IIC_SendCommondToOled(0xA1);
	IIC_SendCommondToOled(0xC8);
	IIC_SendCommondToOled(0xDA);
	IIC_SendCommondToOled(0x12);
	IIC_SendCommondToOled(0x81);
	IIC_SendCommondToOled(0xCF);
	IIC_SendCommondToOled(0xD9);
	IIC_SendCommondToOled(0xF1);
	IIC_SendCommondToOled(0xDB);
	IIC_SendCommondToOled(0x30);
	IIC_SendCommondToOled(0xA4);
	IIC_SendCommondToOled(0xA6);
	IIC_SendCommondToOled(0x8D);
	IIC_SendCommondToOled(0x14);
	IIC_SendCommondToOled(0xAF); // 开启显示
}

//点亮一整行
uint8_t OLED_DisplayALine(void)
{
	//点亮第一页（第一行）
	IIC_SendCommondToOled(0xB0);//发送到第一页
	
	IIC_Start();
	IIC_SendByte(0x78);
	if(IIC_WaitAck())
	goto err;
	
	IIC_SendByte(0x40);//要发送数据
	if(IIC_WaitAck())
	goto err;
	
	for(uint8_t i = 0;i < 128;i++)
	{
		IIC_SendByte(0xFF);
		if(IIC_WaitAck())
	  goto err;
	
	}
	
	IIC_Stop();
	return 0;
	
	err:
		IIC_Stop();
		return 1;
}

/* OLED页写*/
uint8_t OLED_WritePage(uint8_t *data,uint8_t count)
{
	if(count == 0 || count > 128)
	{
		return -1;
	}
	IIC_Start();
	IIC_SendByte(0x78);
	if(IIC_WaitAck())
	  goto err;
	
	IIC_SendByte(0x40);
	if(IIC_WaitAck())
	  goto err;
	
	for(uint8_t j = 0;j < count;j ++)
	{
		IIC_SendByte(data[j]);
		if(IIC_WaitAck())
	  goto err;
	}
	IIC_Stop();
	return 0;
	
	err:
		IIC_Stop();
		return 1;
}

/* 设置读写位置 */
void SetPos(uint8_t page,uint8_t x)
{
	IIC_SendCommondToOled(0xB0 | page);
	
	//设置列的高位
	IIC_SendCommondToOled(0x10 | (x >>= 4));
	
	//设置列的低位
	IIC_SendCommondToOled(0x00 | (x & 0X0F));
}

/* 写整个屏幕 */
void OLED_ALLPage(void)
{
	uint8_t i;
	for(i = 0;i < 8;i ++)
	{
		SetPos(i,0);
		OLED_WritePage(oled_arr[i],128);
	}
}


/*OLED清屏函数 */
void OLED_Clear(void)
{
	for(uint8_t i = 0;i < 8;i++)
	{
		for(uint8_t j = 0;j < 128;j++)
		{
			//写一页数据
			oled_arr[i][j] = 0x00;
		}
	}
	OLED_ALLPage();
}

