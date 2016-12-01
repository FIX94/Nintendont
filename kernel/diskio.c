/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2016        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "diskio.h"
#include "string.h"
#include "debug.h"
#include "Config.h"

#include "SDI.h"
#include "usbstorage.h"
#include "alloc.h"
#include "common.h"

extern bool access_led;
u32 s_size;	// Sector size.
u32 s_cnt;	// Sector count.


/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/
DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
	return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
	/* Nintendont initializes devices outside of FatFS. */
	return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s) [SD card]                                              */
/*-----------------------------------------------------------------------*/

DRESULT disk_read_sd (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	s32 Retry=10;

	//turn on drive led
	if (access_led) set32(HW_GPIO_OUT, GPIO_SLOT_LED);

	while(1)
	{
		if( sdio_ReadSectors( sector, count, buff ) )
			break;

		Retry--;
		if( Retry < 0 )
		{
			//turn off drive led
			if (access_led) clear32(HW_GPIO_OUT, GPIO_SLOT_LED);
			return RES_ERROR;
		}
	}

	//turn off drive led
	if (access_led) clear32(HW_GPIO_OUT, GPIO_SLOT_LED);

	return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s) [SD card]                                             */
/*-----------------------------------------------------------------------*/

DRESULT disk_write_sd (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	//turn on drive led
	if (access_led) set32(HW_GPIO_OUT, GPIO_SLOT_LED);

	if( sdio_WriteSectors( sector, count, buff ) < 0 )
	{
		//turn off drive led
		if (access_led) clear32(HW_GPIO_OUT, GPIO_SLOT_LED);
		return RES_ERROR;
	}

	//turn off drive led
	if (access_led) clear32(HW_GPIO_OUT, GPIO_SLOT_LED);

	return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s) [USB storage]                                          */
/*-----------------------------------------------------------------------*/

DRESULT disk_read_usb (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Sector address in LBA */
	UINT count		/* Number of sectors to read */
)
{
	//turn on drive led
	if (access_led) set32(HW_GPIO_OUT, GPIO_SLOT_LED);

	if(USBStorage_ReadSectors(sector, count, buff) != 1)
	{
		//turn off drive led
		if (access_led) clear32(HW_GPIO_OUT, GPIO_SLOT_LED);
		dbgprintf("USB:Failed to read from USB device... Sector: %d Count: %d dst: %p\r\n", sector, count, buff);
		return RES_ERROR;
	}

	//turn off drive led
	if (access_led) clear32(HW_GPIO_OUT, GPIO_SLOT_LED);

	return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s) [USB storage]                                         */
/*-----------------------------------------------------------------------*/

DRESULT disk_write_usb (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Sector address in LBA */
	UINT count			/* Number of sectors to write */
)
{
	//turn on drive led
	if (access_led) set32(HW_GPIO_OUT, GPIO_SLOT_LED);

	if(USBStorage_WriteSectors(sector, count, buff) != 1)
	{
		//turn off drive led
		if (access_led) clear32(HW_GPIO_OUT, GPIO_SLOT_LED);

		dbgprintf("USB: Failed to write to USB device... Sector: %d Count: %d dst: %p\r\n", sector, count, buff);
		return RES_ERROR;
	}

	//turn off drive led
	if (access_led) clear32(HW_GPIO_OUT, GPIO_SLOT_LED);

	return RES_OK;
}


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	if(cmd == GET_SECTOR_SIZE)
		*(WORD*)buff = s_size;

	return RES_OK;
}


//we cant include time.h so hardcode what we need
struct tm
{
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
};
extern struct tm *gmtime(u32 *time);

// Get the current system time as a FAT timestamp.
DWORD get_fattime(void)
{
	u32 time = GetCurrentTime();
	struct tm *tmp = gmtime(&time);
	DWORD ret =
		((DWORD)(tmp->tm_year - 80) << 25)
		| ((DWORD)(tmp->tm_mon + 1) << 21)
		| ((DWORD)tmp->tm_mday  << 16)
		| ((DWORD)tmp->tm_hour << 11)
		| ((DWORD)tmp->tm_min << 5)
		| ((DWORD)tmp->tm_sec >> 1);
	return ret;
}


/*-----------------------------------------------------------------------*/
/* Nintendont kernel: Device type selection.                             */
/*-----------------------------------------------------------------------*/

DiskReadFunc disk_read;
DiskWriteFunc disk_write;
void SetDiskFunctions(DWORD usb)
{
	if(usb == 1)
	{
		disk_read = disk_read_usb;
		disk_write = disk_write_usb;
	}
	else
	{
		disk_read = disk_read_sd;
		disk_write = disk_write_sd;
	}
}
