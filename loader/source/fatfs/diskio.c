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

/* simple but very effective sector cache */
#define CACHE_SIZE 64
typedef struct _fatCache {
	bool valid;
	DWORD sectorNum;
	BYTE data[_MAX_SS];
} fatCache;
static fatCache cache[_VOLUMES][CACHE_SIZE];
static u32 cachePos[_VOLUMES];


/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive number to identify the drive */
)
{
	/* Clear out cache so we wont get any false positives */
	u32 i;
	for(i = 0; i < CACHE_SIZE; i++)
		cache[pdrv][i].valid = false;
	cachePos[pdrv] = 0;
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
	/* Only cache on single sectors */
	if(count == 1)
	{
		u32 i;
		for(i = 0; i < CACHE_SIZE; i++)
		{
			if(cache[pdrv][i].valid == true && 
				cache[pdrv][i].sectorNum == sector)
			{
				memcpy(buff,cache[pdrv][i].data,__sector_size);
				return RES_OK;
			}
		}
	}
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
	/* Got done reading sector, copy to cache */
	if(count == 1)
	{
		u32 cPos = cachePos[pdrv];
		cache[pdrv][cPos].valid = true;
		cache[pdrv][cPos].sectorNum = sector;
		memcpy(cache[pdrv][cPos].data,buff,__sector_size);
		cPos++;
		/* Wrap cache around */
		if(cPos == CACHE_SIZE)
			cPos = 0;
		cachePos[pdrv] = cPos;
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
	/* Make sure to invalidate cache on changes */
	u32 i;
	for(i = 0; i < CACHE_SIZE; i++)
	{
		if(cache[pdrv][i].valid == true &&
			cache[pdrv][i].sectorNum >= sector && 
			cache[pdrv][i].sectorNum < (sector+count))
			cache[pdrv][i].valid = false;
	}

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
