// FIXME: Preliminary loader diskio.c for Nintendont.
// Needs cleaning up before committing.

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
#include "ff.h"

extern DISC_INTERFACE __io_wiisd;
extern DISC_INTERFACE __io_custom_usbstorage;
DISC_INTERFACE *driver[_VOLUMES] = { &__io_wiisd, &__io_custom_usbstorage };

//from usbstorage.c
extern u32 __sector_size;

typedef enum {
	DEV_SD	= 0,
	DEV_USB	= 1,
} DeviceNumber;

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/
DSTATUS disk_status (
	BYTE pdrv		/* Physical drive number to identify the drive */
)
{
	// TODO
	return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive number to identify the drive */
)
{
	if (pdrv < DEV_SD || pdrv > DEV_USB)
		return RES_PARERR;

	/* Nintendont initializes devices outside of FatFS. */
	// TODO: Move device initialization here?
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
	if (pdrv < DEV_SD || pdrv > DEV_USB)
		return RES_PARERR;

	int retry = 10;
	for (; retry >= 0; retry--)
	{
		if (driver[pdrv]->readSectors(sector, count, buff))
			break;
	}

	if (retry < 0)
		return RES_ERROR;

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
	if (pdrv < DEV_SD || pdrv > DEV_USB)
		return RES_PARERR;

	if (driver[pdrv]->writeSectors(sector, count, buff) < 0)
		return RES_ERROR;

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
	int ret = RES_PARERR;
	if (pdrv < DEV_SD || pdrv > DEV_USB)
		return ret;

	switch (cmd) {
		case GET_SECTOR_SIZE:
			if (pdrv == 0) {
				// Hardcoded SD sector size.
				*(WORD*)buff = 512;
			} else {
				// USB sector size.
				*(WORD*)buff = __sector_size;
			}
			ret = RES_OK;
			break;

		case GET_SECTOR_COUNT:
			// TODO?
			break;

		default:
			break;
	}

	return ret;
}



// Get the current system time as a FAT timestamp.
DWORD get_fattime(void)
{
	time_t now;
	struct tm tm;

	// TODO: Return a default timestamp on error?
	if (time(&now) == (time_t)-1) {
		// time() failed.
		return 0;
	}
	if (!localtime_r(&now, &tm)) {
		// localtime_r() failed.
		return 0;
	}

	/**
	 * Convert to an MS-DOS timestamp.
	 * Reference: http://elm-chan.org/fsw/ff/en/fattime.html
	 * Bits 31-25: Year. (base is 1980) (0..127)
	 * Bits 24-21: Month. (1..12)
	 * Bits 20-16: Day. (1..31)
	 * Bits 15-11: Hour. (0..23)
	 * Bits 10-5:  Minute. (0..59)
	 * Bits 4-0:   Seconds/2. (0..29)
	 */
	return (((tm.tm_year - 80) & 0x7F) << 25) |	// tm.tm_year base is 1900, not 1980.
	       (((tm.tm_mon + 1) & 0xF) << 21) |	// tm.tm_mon starts at 0, not 1.
	       ((tm.tm_mday & 0x1F) << 16) |
	       ((tm.tm_hour & 0x1F) << 11) |
	       ((tm.tm_min & 0x3F) << 5) |
	       ((tm.tm_sec / 2) & 0x1F);
}
