/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2016        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "diskio.h"
#include "global.h"
#include "Config.h"

extern DISC_INTERFACE __io_wiisd;
extern DISC_INTERFACE __io_custom_usbstorage;
DISC_INTERFACE *driver[_VOLUMES] = { &__io_wiisd, &__io_custom_usbstorage };

//from usbstorage.c
extern u32 __sector_size;

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/
DSTATUS disk_status (
	BYTE pdrv		/* Physical drive number to identify the drive */
)
{
	return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive number to identify the drive */
)
{
	/* Nintendont initializes devices outside of FatFS. */
	return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                           */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive number to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Sector address in LBA */
	UINT count		/* Number of sectors to read */
)
{
	s32 Retry=10;

	while(1)
	{
		if( driver[pdrv]->readSectors( sector, count, buff ) )
			break;

		Retry--;
		if( Retry < 0 )
		{
			return RES_ERROR;
		}
	}

	return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                            */
/*-----------------------------------------------------------------------*/

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive number to identify the drive */
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Sector address in LBA */
	UINT count			/* Number of sectors to write */
)
{
	if( driver[pdrv]->writeSectors( sector, count, buff ) < 0 )
	{
		return RES_ERROR;
	}

	return RES_OK;
}


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive number (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	if(cmd == GET_SECTOR_SIZE)
	{
		if(pdrv == 0)
			*(WORD*)buff = 512; //hardcoded sd
		else
			*(WORD*)buff = __sector_size;
	}
	return RES_OK;
}



// Get the current system time as a FAT timestamp.
DWORD get_fattime(void)
{
	// TODO: Implement this for Nintendont.
	//rtc_time tm;
	//DWORD ret;

	//MkTime(&tm);
	//ret =
	//	((DWORD)(tm.tm_year + 20) << 25)
	//	| ((DWORD)(tm.tm_mon + 1) << 21)
	//	| ((DWORD)tm.tm_mday  << 16)
	//	| ((DWORD)tm.tm_hour << 11)
	//	| ((DWORD)tm.tm_min << 5)
	//	| ((DWORD)tm.tm_sec >> 1);
	//return ret;

	return 0;
}
