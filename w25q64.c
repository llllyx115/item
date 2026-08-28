#define LOG_TAG "w25q64"

#include "stm32f4xx.h"                  // Device header
#include "stdio.h"
#include "mydelay.h"
#include "ff.h"
#include "myspi_sys.h"
#include "w25q64.h"
#include "elog.h"

/* 指令定义 */
#define CMD_WRITE_ENABLE    0x06
#define CMD_READ_STATUS1    0x05
#define CMD_PAGE_PROGRAM    0x02
#define CMD_SECTOR_ERASE    0x20
#define CMD_CHIP_ERASE      0xC7
#define CMD_READ_DATA       0x03
#define CMD_JEDEC_ID        0x9F

void W25Q64_Init(void)
{
    MySPI_Init();
}

/* 忙等待: 返回 0=空闲, 1=超时 */
uint8_t W25Q64_BusyWait(void)
{
    uint8_t status;
    uint32_t timeout = 400000;   /* 400ms, 单位us */

    MySPI_Start();
    MySPI_SwapByte(CMD_READ_STATUS1);

    do{
        status = MySPI_SwapByte(0xFF);
        if((status & 0x01) == 0x00) break;   /* BUSY位清零，操作完成 */
        MyDelay_us(20);
        timeout -= 20;
    }while(timeout > 0);

    MySPI_Stop();
    return (status & 0x01) ? 1 : 0;
}

/* 整片擦除专用: 耗时可达数十秒，单独放宽超时 */
static uint8_t W25Q64_BusyWaitLong(void)
{
    uint8_t status;
    uint32_t timeout = 60000;    /* 60秒, 单位ms */

    MySPI_Start();
    MySPI_SwapByte(CMD_READ_STATUS1);

    do{
        status = MySPI_SwapByte(0xFF);
        if((status & 0x01) == 0x00) break;
        MyDelay_ms(1);
        timeout--;
    }while(timeout > 0);

    MySPI_Stop();
    return (status & 0x01) ? 1 : 0;
}

void W25Q64_WriteEnable(void)
{
    MySPI_Start();
    MySPI_SwapByte(CMD_WRITE_ENABLE);
    MySPI_Stop();
}

uint32_t W25Q64_ReadID(void)
{
    uint32_t data;

    MySPI_Start();
    MySPI_SwapByte(CMD_JEDEC_ID);
    data  = MySPI_SwapByte(0xFF);
    data <<= 8;
    data |= MySPI_SwapByte(0xFF);
    data <<= 8;
    data |= MySPI_SwapByte(0xFF);
    MySPI_Stop();

    return data;
}

uint8_t W25Q64_SectorErase(uint32_t address)
{
    W25Q64_WriteEnable();

    MySPI_Start();
    MySPI_SwapByte(CMD_SECTOR_ERASE);
    MySPI_SwapByte(address >> 16);
    MySPI_SwapByte(address >> 8);
    MySPI_SwapByte(address);
    MySPI_Stop();

    return W25Q64_BusyWait();
}

uint8_t W25Q64_ChipErase(void)
{
    W25Q64_WriteEnable();

    MySPI_Start();
    MySPI_SwapByte(CMD_CHIP_ERASE);
    MySPI_Stop();

    return W25Q64_BusyWaitLong();
}

/* 页写入: 不支持跨页，count 最大 256 */
void W25Q64_PageWrite(uint32_t address, uint8_t *data, uint16_t count)
{
    W25Q64_WriteEnable();

    MySPI_Start();
    MySPI_SwapByte(CMD_PAGE_PROGRAM);
    MySPI_SwapByte(address >> 16);
    MySPI_SwapByte(address >> 8);
    MySPI_SwapByte(address);

    MySPI_SendBuf(data, count);      /* 批量发送 */

    MySPI_Stop();
    W25Q64_BusyWait();
}

void W25Q64_ReadDate(uint32_t address, uint8_t *arr, uint32_t count)
{
    MySPI_Start();
    MySPI_SwapByte(CMD_READ_DATA);
    MySPI_SwapByte(address >> 16);
    MySPI_SwapByte(address >> 8);
    MySPI_SwapByte(address);

    MySPI_RecvBuf(arr, count);       /* 批量接收 */

    MySPI_Stop();
}

/* 不检查是否需要擦除，自动处理跨页 */
void W25Q64_WriteNoCheck(const uint8_t *buffer, uint32_t WriteAddr, uint32_t NumByteToWrite)
{
    uint32_t pageremain;

    if(NumByteToWrite == 0) return;

    pageremain = W25Q64_PAGE_SIZE - (WriteAddr % W25Q64_PAGE_SIZE);
    if(NumByteToWrite <= pageremain) pageremain = NumByteToWrite;

    while(NumByteToWrite > 0)
    {
        W25Q64_PageWrite(WriteAddr, (uint8_t*)buffer, pageremain);

        NumByteToWrite -= pageremain;
        buffer         += pageremain;
        WriteAddr      += pageremain;

        pageremain = (NumByteToWrite >= W25Q64_PAGE_SIZE)
                     ? W25Q64_PAGE_SIZE : NumByteToWrite;
    }
}

uint8_t W25Q64_SECTOR_BUF[W25Q64_SECTOR_SIZE];

void W25Q64_Write_janwenl(uint8_t *pBuffer, uint32_t WriteAddr, uint32_t NumByteToWrite)
{
    uint32_t secpos, secoff, secremain;
    uint32_t i;
    uint8_t *W25Q64_BUF = W25Q64_SECTOR_BUF;

    if(NumByteToWrite == 0) return;

    secpos    = WriteAddr / W25Q64_SECTOR_SIZE;
    secoff    = WriteAddr % W25Q64_SECTOR_SIZE;
    secremain = W25Q64_SECTOR_SIZE - secoff;
    if(NumByteToWrite <= secremain) secremain = NumByteToWrite;

    while(1)
    {
        /* 快速通道: 整扇区对齐覆盖写，跳过读回判断 */
        if(secoff == 0 && secremain == W25Q64_SECTOR_SIZE)
        {
            W25Q64_SectorErase(secpos * W25Q64_SECTOR_SIZE);
            W25Q64_WriteNoCheck(pBuffer, secpos * W25Q64_SECTOR_SIZE, W25Q64_SECTOR_SIZE);
        }
        else
        {
            /* 部分写: 读-改-写 */
            W25Q64_ReadDate(secpos * W25Q64_SECTOR_SIZE, W25Q64_BUF, W25Q64_SECTOR_SIZE);

            for(i = 0; i < secremain; i++)
            {
                if(W25Q64_BUF[secoff + i] != 0xFF) break;
            }

            if(i < secremain)   /* 有旧数据，需要擦除 */
            {
                W25Q64_SectorErase(secpos * W25Q64_SECTOR_SIZE);
                for(i = 0; i < secremain; i++)
                {
                    W25Q64_BUF[secoff + i] = pBuffer[i];
                }
                W25Q64_WriteNoCheck(W25Q64_BUF, secpos * W25Q64_SECTOR_SIZE, W25Q64_SECTOR_SIZE);
            }
            else                /* 区域干净，直接写 */
            {
                W25Q64_WriteNoCheck(pBuffer, WriteAddr, secremain);
            }
        }

        if(NumByteToWrite == secremain) break;

        secpos++;
        secoff     = 0;
        pBuffer   += secremain;
        WriteAddr += secremain;
        NumByteToWrite -= secremain;
        secremain  = (NumByteToWrite > W25Q64_SECTOR_SIZE)
                     ? W25Q64_SECTOR_SIZE : NumByteToWrite;
    }
}
//void W25Q64_Init(void)
//{
//	//初始化SPI
//	MySPI_Init();
//	
//}

////忙等待
//uint8_t W25Q64_BusyWait(void)
//{
//	uint8_t status;
//	uint32_t time = 3000;
//	
//	MySPI_Start();
//	MySPI_SwapByte(0x05);
//	
//	do{		
//		status = MySPI_SwapByte(0xFF);
//		MySPI_Stop();
//		
//		if((status & 0x01) == 0){
//			return 0;
//		}
//		
//		MyDelay_ms(1);
//		time--;		
//	}while(time>0);
//	return 1;
//}

////写使能---擦除 或 写
//void W25Q64_WriteEnable(void)
//{
//	MySPI_Start();
//	MySPI_SwapByte(0x06);
//	MySPI_Stop();	
//}

////读设备ID
//uint32_t W25Q64_ReadID(void)
//{
//	uint32_t data;
//	
//	MySPI_Start();
//	
//	MySPI_SwapByte(0x9f);
//	
//	data = MySPI_SwapByte(0xFF);
//	data <<= 8;
//	data |= MySPI_SwapByte(0xFF);
//	data <<= 8;
//	data |= MySPI_SwapByte(0xFF);
//	MySPI_Stop();
//	return data;
//}
///* 扇区擦除 */
//uint8_t W25Q64_SectorErase(uint32_t address)
//{
//	//写使能
//	W25Q64_WriteEnable();
//	
//	//0x20
//	MySPI_Start();
//	MySPI_SwapByte(0x20);
//	
//	//地址
//	MySPI_SwapByte(address >> 16);
//	MySPI_SwapByte(address >> 8);
//	MySPI_SwapByte(address);
//	
//	MySPI_Stop();
//	if(W25Q64_BusyWait() == 0){
//		return 0;
//	}
//	else{
//		return 1;
//	}
//	
//}
///*  整片擦除 */
//uint8_t W25Q64_ChipErase(void)
//{
//	uint8_t status;
//	W25Q64_WriteEnable();
//	
//	MySPI_Start();
//	MySPI_SwapByte(0xC7);
//	MySPI_Stop();
//	
//	// 整片擦除专用等待：不限次数，直到 BUSY 位清零
//	do{
//		MySPI_Start();
//		MySPI_SwapByte(0x05);
//		status = MySPI_SwapByte(0xFF);
//		MySPI_Stop();
//	}while(status & 0x01);
//	
//	return 0;
//}
///*
//	页写 (不支持跨页写） 256
//	没有实现跨页写 所以，地址直接给0x00
//*/
//void W25Q64_PageWrite(uint32_t address,uint8_t *data,uint16_t count)
//{
//			//写使能
//		W25Q64_WriteEnable();
//		
//		MySPI_Start();
//		MySPI_SwapByte(0x02);
//		
//		//地址
//		MySPI_SwapByte(address >> 16);
//		MySPI_SwapByte(address >> 8);
//		MySPI_SwapByte(address);
//		
//		for(uint32_t i = 0; i < count; i++)
//		{
//			MySPI_SwapByte(data[i]);		
//		}
//		MySPI_Stop();
//		W25Q64_BusyWait();
//}

///* 读数据 */
//void W25Q64_ReadDate(uint32_t address,uint8_t *arr,uint32_t count)
//{
//	MySPI_Start();
//	MySPI_SwapByte(0x03);
//	
//	//地址
//	MySPI_SwapByte(address >> 16);
//	MySPI_SwapByte(address >> 8);
//	MySPI_SwapByte(address);
//	
//	for(uint32_t i = 0; i < count; i++)
//	{
//		arr[i] = MySPI_SwapByte(0xFF);		
//	}
//	MySPI_Stop();
//}


//void W25Q64_WriteNoCheck(const uint8_t *buffer, uint32_t WriteAddr, uint32_t NumByteToWrite)
//{
//    uint32_t pageremain; // 当前页剩余的字节数
//    uint32_t write_len;  // 本次循环要写入的长度

//    // 计算当前页剩余的空间
//    // 例如: 地址 250，页大小 256，剩余 6 字节
//    pageremain = 256 - (WriteAddr % 256);

//    // 如果要写的数据很少，连当前页都填不满
//    if (NumByteToWrite <= pageremain) 
//    {
//        pageremain = NumByteToWrite;
//    }
//    while (NumByteToWrite > 0)
//    {
//        // 1. 执行页写入
//        W25Q64_PageWrite(WriteAddr, (uint8_t*)buffer, pageremain);

//        // 2. 更新指针和计数器
//        NumByteToWrite -= pageremain; // 减去已写的
//        buffer         += pageremain; // 数据指针后移
//        WriteAddr      += pageremain; // Flash地址后移

//        // 3. 计算下一次写入长度
//        // 下一次肯定是从新的页首开始写，所以剩余空间直接是 256
//        if (NumByteToWrite >= 256)
//        {
//            pageremain = 256; 
//        }
//        else 
//        {
//            pageremain = NumByteToWrite;
//        }
//    }
//}

//uint8_t W25Q64_SECTOR_BUF[4096];  //扇区缓冲

//void W25Q64_Write_janwenl(uint8_t *pBuffer, uint32_t WriteAddr, uint32_t NumByteToWrite)
//{
//    uint32_t secpos;       // 扇区地址
//    uint32_t secoff;       // 扇区内的偏移
//    uint32_t secremain;    // 扇区剩余空间
//    uint16_t i;
//    uint8_t *W25Q64_BUF;

//    W25Q64_BUF = W25Q64_SECTOR_BUF;
//    
//    // 1. 计算地址信息
//    secpos = WriteAddr / 4096; // 算出所在的扇区编号
//    secoff = WriteAddr % 4096; // 算出在扇区内的偏移位置
//    secremain = 4096 - secoff; // 算出这个扇区还剩下多少空间

//    // 如果要写的数据长度小于当前扇区的剩余空间，说明一个扇区就能装下
//    if(NumByteToWrite <= secremain) 
//    {
//        secremain = NumByteToWrite;
//    }
//    while(1)
//    {
//        // 2. 读出整个扇区的内容到 RAM 缓存中
//        W25Q64_ReadDate(secpos * 4096, W25Q64_BUF, 4096); 
//        
//        // 3. 检查我们要写入的区域，是不是已经全都是 0xFF (被擦除过了)
//        for(i = 0; i < secremain; i++)
//        {
//            if(W25Q64_BUF[secoff + i] != 0xFF) 
//            {
//                break; // 只要碰到一个不是 0xFF 的，说明有老数据，必须擦除
//            }
//        }        
//        // 4. 根据检查结果执行写入
//        if(i < secremain) // 需要擦除
//        {
//            W25Q64_SectorErase(secpos * 4096); // 擦除目标物理扇区
//            
//            // 把我们要写的新数据，覆盖到 RAM 缓存里的对应位置
//            for(i = 0; i < secremain; i++)
//            {
//                W25Q64_BUF[i + secoff] = pBuffer[i];
//            }
//            
//            // 把拼接好的 4096 字节缓存数据，整体重新写回 Flash
//            W25Q64_WriteNoCheck(W25Q64_BUF, secpos * 4096, 4096);
//			
//        }
//        else 
//        {
//            // 不需要擦除，目标区域是干净的，直接在偏移地址写入即可
//            W25Q64_WriteNoCheck(pBuffer, WriteAddr, secremain);
//        }
//        // 5. 判断数据是否全部写完，还是需要跨入下一个扇区
//        if(NumByteToWrite == secremain) 
//        {
//            break; // 全部写完了，跳出循环
//        }
//        else // 还没写完，说明跨扇区了
//        {
//            secpos++;                   // 扇区编号 +1，进入下一个物理扇区
//            secoff = 0;                 // 既然是新扇区，偏移肯定是从 0 开始
//            pBuffer += secremain;       // 数据指针往后挪
//            WriteAddr += secremain;     // 写入地址往后挪
//            NumByteToWrite -= secremain;// 剩余字节数递减
//            
//            // 计算下一个扇区还要写多少
//            if(NumByteToWrite > 4096) 
//            {
//                secremain = 4096;       // 下一个扇区还是填不满，还得继续跨
//            }
//            else 
//            {
//                secremain = NumByteToWrite; // 下一个扇区终于能写完了
//            }
//        }
//    }
//}

//FatF挂载测试函数
	
FATFS fs;              // 文件系统对象，全局只需要一份
static uint8_t fs_mounted = 0;   // 标记当前是否已经挂载成功

/**
 * @brief  初始化W25Q64并挂载FatFs文件系统
 *         已有文件系统 -> 直接挂载
 *         没有文件系统 -> 格式化后再挂载
 * @return 0=成功  1=失败
 */
uint8_t FatFs_Setup(void)
{
	FRESULT fr;
	//BYTE work[FF_MAX_SS];   // 格式化用的工作缓冲区

	/* 1. 初始化SPI/W25Q64硬件 */
	//W25Q64_Init();

	/* 2. 确认通信正常 */
	uint32_t id = W25Q64_ReadID();
	if(id == 0x000000 || id == 0xFFFFFF)
	{
		log_e("W25Q64 Communication fail, ID=0x%06X\r\n", id);
		return 1;
	}	
	/* 2. 尝试直接挂载 */
	fr = f_mount(&fs, "0:", 1);
	if(fr == FR_OK)
	{
		log_d("fatfs successful\r\n");
		fs_mounted = 1;
		return 0;
	}

	/* 4. 没有文件系统 或 挂载异常 -> 格式化 */
	if(fr == FR_NO_FILESYSTEM)
	{
		//先擦除整片
		log_d("Chip Erase...\n");
		W25Q64_ChipErase();
		log_d("chip successful\n");
		
		log_d("find no fatfs system,formating...\r\n");

		fr = f_mkfs("0:", 0,W25Q64_SECTOR_BUF, 4096);
		if(fr != FR_OK)
		{
			log_e("format fail,error: %d\r\n", fr);
			return 1;
		}
		log_d("format end\r\n");

		/* 格式化后重新挂载 */
		fr = f_mount(&fs, "0:", 1);
		if(fr != FR_OK)
		{
			log_e("fatfs fail after format, error: %d\r\n", fr);
			return 1;
		}

		log_d("fatfs successful\r\n");
		fs_mounted = 1;
		return 0;
	}
	log_e("format fail, error: %d\r\n", fr);
	return 1;
}

/* 写文件 */
void MyFile_WriteTest(void)
{
	if(!fs_mounted) return;   // 防御性检查
	
	FIL file;
	UINT bw;
	FRESULT fr;

	fr = f_open(&file, "0:test.txt", FA_CREATE_ALWAYS | FA_WRITE);
	if(fr != FR_OK)
	{
		printf("open file fail: %d\n", fr);
		return;
	}
	printf("test write\n");
	char *data = "Hello lyx!\n";
	fr = f_write(&file, data, strlen(data), &bw);
	printf("write: %d, bytes: %d\r\n", fr, bw);

	f_close(&file);
}

void MyFile_ReadTest(void)
{
	if(!fs_mounted) return;

	FIL file;
	UINT br;
	char buf[128] = {0};
	FRESULT fr;

	fr = f_open(&file, "0:test.txt", FA_READ);
	if(fr != FR_OK)
	{
		printf("open file fail: %d\r\n", fr);
		return;
	}

	fr = f_read(&file, buf, sizeof(buf) - 1, &br);
	printf("read: %d, string: %s\r\n", fr, buf);

	f_close(&file);
}



