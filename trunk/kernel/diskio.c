#include "diskio.h"
#include "string.h"
#include "debug.h"
#include "Config.h"

#include "SDI.h"
#include "syscalls.h"
#include "usbstorage.h"
#include "alloc.h"
#include "common.h"

u32 s_size;
u32 s_cnt;

DSTATUS disk_initialize_sd( BYTE drv, WORD *ss )
{
	*ss = PAGE_SIZE512;
	return RES_OK;
}

DRESULT disk_read_sd( BYTE drv, BYTE *buff, DWORD sector, BYTE count )
{
	s32 Retry=10;

	if (ConfigGetConfig(NIN_CFG_LED))
		set32(HW_GPIO_OUT, GPIO_SLOT_LED);	//turn on drive light

	while(1)
	{
		if( sdio_ReadSectors( sector, count, buff ) )
			break;

		Retry--;
		if( Retry < 0 )
		{
			if (ConfigGetConfig(NIN_CFG_LED))
				clear32(HW_GPIO_OUT, GPIO_SLOT_LED); //turn off drive light
			return RES_ERROR;
		}
	}

	if (ConfigGetConfig(NIN_CFG_LED))
		clear32(HW_GPIO_OUT, GPIO_SLOT_LED); //turn off drive light

	return RES_OK;
}
// Write Sector(s)
DRESULT disk_write_sd( BYTE drv, const BYTE *buff, DWORD sector, BYTE count )
{
	if( sdio_WriteSectors( sector, count, buff ) < 0 )
		return RES_ERROR;

	return RES_OK;
}

DSTATUS disk_initialize_usb(BYTE drv, WORD *ss)
{
	*ss = s_size;
	return RES_OK;
}

DRESULT disk_read_usb(BYTE drv, BYTE *buff, DWORD sector, BYTE count)
{
	if (ConfigGetConfig(NIN_CFG_LED))
		set32(HW_GPIO_OUT, GPIO_SLOT_LED);	//turn on drive light

	if(USBStorage_ReadSectors(sector, count, buff) != 1)
	{
		if (ConfigGetConfig(NIN_CFG_LED))
			clear32(HW_GPIO_OUT, GPIO_SLOT_LED); //turn off drive light
		dbgprintf("USB:Failed to read from USB device... Sector: %d Count: %d dst: %p\r\n", sector, count, buff);
		return RES_ERROR;
	}

	if (ConfigGetConfig(NIN_CFG_LED))
		clear32(HW_GPIO_OUT, GPIO_SLOT_LED); //turn off drive light

	return RES_OK;
}

DRESULT disk_write_usb(BYTE drv, const BYTE *buff, DWORD sector, BYTE count)
{
	if(USBStorage_WriteSectors(sector, count, buff) != 1)
	{
		dbgprintf("USB: Failed to write to USB device... Sector: %d Count: %d dst: %p\r\n", sector, count, buff);
		return RES_ERROR;
	}

	return RES_OK;
}

DSTATUS disk_status( BYTE drv )
{
	(void)drv;
	return RES_OK;
}

DWORD get_fattime(void)
{
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

DiskInitFunc disk_initialize;
DiskReadFunc disk_read;
DiskWriteFunc disk_write;
void SetDiskFunctions(DWORD usb)
{
	if(usb == 1)
	{
		disk_initialize = disk_initialize_usb;
		disk_read = disk_read_usb;
		disk_write = disk_write_usb;
	}
	else
	{
		disk_initialize = disk_initialize_sd;
		disk_read = disk_read_sd;
		disk_write = disk_write_sd;
	}
}
