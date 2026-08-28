/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "ff.h"			/* Obtains integer types */
#include "diskio.h"		/* Declarations of disk functions */
#include "w25q64.h"

/* Definitions of physical drive number for each drive */
//#define DEV_RAM		0	/* Example: Map Ramdisk to physical drive 0 */
//#define DEV_MMC		1	/* Example: Map MMC/SD card to physical drive 1 */
//#define DEV_USB		2	/* Example: Map USB MSD to physical drive 2 */

#define SPI_FLASH 0  //设备号
extern uint8_t W25Q64_SECTOR_BUF[4096];  //扇区缓冲

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

//获取驱动器的状态
DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
	if(pdrv== SPI_FLASH)
	{
		return 0; //状态正常
	}
	return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* 初始化磁盘驱动器                                                  */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
	if(pdrv == SPI_FLASH){
		W25Q64_Init();
		return 0;
	}
	return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/*  磁盘驱动号 */
	BYTE *buff,		/*  缓存区 */
	LBA_t sector,	/*  扇区号 */
	UINT count		/*  扇区个数 */
)
{
	if(pdrv == SPI_FLASH)
	{
		W25Q64_ReadDate(sector*4096,buff,count * 4096);
		return RES_OK;
	}
	return RES_PARERR;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (
	BYTE pdrv,			/*  */
	const BYTE *buff,	/* 发送缓冲区 */
	LBA_t sector,		/* 扇区号*/
	UINT count			/* 扇区个数 */
)
{
	if(pdrv == SPI_FLASH)
	{
	
	if(sector >= 2048 || (sector + count) > 2048)
	{
		return RES_PARERR;
	}
		W25Q64_Write_janwenl((BYTE *)buff,sector * 4096,count*4096);
		return RES_OK;
	}
	return RES_PARERR;
}

#endif


/*-----------------------------------------------------------------------*/
/* 格式化 获取容量                                             */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	if(pdrv == SPI_FLASH){
		switch(cmd){
			//扇区总数
			case GET_SECTOR_COUNT:
				*(LBA_t *)buff = 2048;	
				return RES_OK;
			case GET_SECTOR_SIZE:
				*(WORD *)buff = 4096;
				return RES_OK;
				//1个扇区
			case GET_BLOCK_SIZE:
				*(DWORD *)buff = 1;
				return RES_OK;
			case CTRL_SYNC:
				W25Q64_BusyWait();
				return RES_OK;
		}				
	}
	return RES_PARERR;
}

