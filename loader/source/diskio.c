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
#include "usb_ogc.h"

#include <limits.h>
#include "ff_cache/cache.h"

extern DISC_INTERFACE __io_wiisd;
extern DISC_INTERFACE __io_custom_usbstorage;
DISC_INTERFACE *driver[_VOLUMES] = { &__io_wiisd, &__io_custom_usbstorage };
static bool disk_isInit[_VOLUMES] = {0};

// Disk cache.
unsigned int sectorSize[_VOLUMES] = {0, 0};
CACHE *cache[_VOLUMES] = {NULL, NULL};

//from usbstorage.c
extern u32 __sector_size;	// USB sector size. (Not known until device init.)
extern void USBStorageOGC_Deinitialize();

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/
DSTATUS disk_status (
	BYTE pdrv		/* Physical drive number to identify the drive */
)
{
	if (pdrv < DEV_SD || pdrv > DEV_USB)
		return STA_NOINIT;

	if (disk_isInit[pdrv]) {
		// TODO: Check write protection on SD?
		return (driver[pdrv]->isInserted() ? 0 : STA_NODISK);
	}

	// Disk isn't initialized.
	return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive number to identify the drive */
)
{
	if (pdrv < DEV_SD || pdrv > DEV_USB)
		return STA_NOINIT;

	// Attempt to initialize the device.
	// NOTE: For USB, this may have to be run multiple times.
	// That should be done by the caller in order to allow
	// for asynchronous startup.
	if (!disk_isInit[pdrv]) {
		if (!driver[pdrv]->startup())
			return STA_NOINIT;
	}
	if (!driver[pdrv]->isInserted())
		return STA_NODISK;

	// Determine the sector size.
	// - USB: 512 or 4096, depending on drive.
	// - SD: Always 512.
	switch (pdrv) {
		case DEV_SD:
		default:
			sectorSize[pdrv] = 512;
			break;
		case DEV_USB:
			sectorSize[pdrv] = __sector_size;
			break;
	}

	// Initialize the disk cache.
	// libfat/source/common.h:
	// - DEFAULT_CACHE_PAGES = 4
	// - DEFAULT_SECTORS_PAGE = 64
	// NOTE: endOfPartition isn't usable, since this is a
	// per-disk cache, not per-partition. Use UINT_MAX-1.
	cache[pdrv] = _FAT_cache_constructor(4, 64, driver[pdrv], UINT_MAX-1, sectorSize[pdrv]);

	// Device initialized.
	disk_isInit[pdrv] = true;
	return 0;
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive number to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	if (pdrv < DEV_SD || pdrv > DEV_USB || count == 0)
		return RES_PARERR;

	// Read from the cache.
	// TODO: Copy the "attempt 10 reads" code to cache.c?
	bool ret;
	if (count == 1) {
		// Single sector.
		ret = _FAT_cache_readSector(cache[pdrv], buff, sector);
	} else {
		// Multiple sectors.
		ret = _FAT_cache_readSectors(cache[pdrv], sector, count, buff);
	}

	return (ret ? RES_OK : RES_ERROR);
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive number to identify the drive */
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	if (pdrv < DEV_SD || pdrv > DEV_USB || count == 0)
		return RES_PARERR;

	// Write to the cache.
	bool ret;
	if (count == 1) {
		// Single sector.
		ret = _FAT_cache_writeSector(cache[pdrv], buff, sector);
	} else {
		// Multiple sectors.
		ret = _FAT_cache_writeSectors(cache[pdrv], sector, count, buff);
	}

	return (ret ? RES_OK : RES_ERROR);
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
			*(WORD*)buff = sectorSize[pdrv];
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



/*-----------------------------------------------------------------------*/
/* Nintendont: Shut down a device.                                       */
/*-----------------------------------------------------------------------*/
DRESULT disk_shutdown (BYTE pdrv)
{
	if (pdrv < DEV_SD || pdrv > DEV_USB)
		return RES_PARERR;
	if (!disk_isInit[pdrv])
		return RES_OK;

	if (cache[pdrv]) {
		// Flush and destroy the cache.
		_FAT_cache_destructor(cache[pdrv]);
		cache[pdrv] = NULL;
	}

	// Shut down the device.
	driver[pdrv]->shutdown();
	if (pdrv == DEV_USB) {
		// Shut down the USB subsystem as well.
		USBStorageOGC_Deinitialize();
		USB_OGC_Deinitialize();
	}
	disk_isInit[pdrv] = false;
	return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Nintendont: Flush the disk cache.                                     */
/*-----------------------------------------------------------------------*/
DRESULT disk_flush (BYTE pdrv)
{
	if (pdrv < DEV_SD || pdrv > DEV_USB)
		return RES_PARERR;
	if (!disk_isInit[pdrv])
		return RES_OK;

	if (cache[pdrv]) {
		// Flush the cache.
		_FAT_cache_flush(cache[pdrv]);
	}

	return RES_OK;
}
