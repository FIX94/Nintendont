#include "diskio.h"
#include "string.h"
#include "debug.h"
#include "Config.h"

#ifndef NINTENDONT_USB
#include "SDI.h"
#include "syscalls.h"
#else
#include "usb.h"
#include "usbstorage.h"
#include "alloc.h"
#include "common.h"
#endif

u32 s_size;
u32 s_cnt;
u32 max_sec = 128;
u8 *buffer = (u8*)NULL;

#ifndef NINTENDONT_USB

DSTATUS disk_initialize( BYTE drv, WORD *ss )
{
	return RES_OK;
}
DSTATUS disk_status( BYTE drv )
{
	return RES_OK;
}
DRESULT disk_read( BYTE drv, BYTE *buff, DWORD sector, BYTE count )
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
DRESULT disk_write( BYTE drv, const BYTE *buff, DWORD sector, BYTE count )
{
	u8 *buffer = (u8*)heap_alloc_aligned( 0, count*512, 0x40 );

	memcpy( buffer, (void*)buff, count*512 );

	_ahbMemFlush( 0xA );

	if( sdio_WriteSectors( sector, count, buffer ) < 0 )
	{
		return RES_ERROR;
	}

	heap_free( 0, buffer );

	return RES_OK;
}

#else

DSTATUS disk_initialize(BYTE drv, WORD *ss)
{
	*ss = s_size;
	max_sec = 0x10000 / s_size; //64kb security buffer
	buffer = (u8*)malloca(0x10000, 32);
	return RES_OK;
}

DSTATUS disk_status(BYTE drv)
{
	(void)drv;
	return RES_OK;
}

DRESULT disk_read(BYTE drv, BYTE *buff, DWORD sector, BYTE count)
{
	u32 t_read = 0;

	if (ConfigGetConfig(NIN_CFG_LED))
		set32(HW_GPIO_OUT, GPIO_SLOT_LED);	//turn on drive light

	while(t_read < count)
	{
		u32 r_sec = (count - t_read);

		if(r_sec > max_sec)
			r_sec = max_sec;

		if(__usbstorage_ReadSectors(sector+t_read, r_sec, buffer) != 1)
		{
			if (ConfigGetConfig(NIN_CFG_LED))
				clear32(HW_GPIO_OUT, GPIO_SLOT_LED); //turn off drive light
			dbgprintf("USB:Failed to read from USB device... Sector: %d Count: %d dst: %p\r\n", sector, count, buff);
			return RES_ERROR;
		}
		memcpy(buff, buffer, r_sec * s_size);
		buff += r_sec * s_size;
		t_read += r_sec;
	}

	if (ConfigGetConfig(NIN_CFG_LED))
		clear32(HW_GPIO_OUT, GPIO_SLOT_LED); //turn off drive light

	return RES_OK;
}

DRESULT disk_write(BYTE drv, const BYTE *buff, DWORD sector, BYTE count)
{	
	u32 t_write = 0;

	while(t_write < count)
	{
		u32 w_sec = (count - t_write);

		if(w_sec > max_sec)
			w_sec = max_sec;

		memcpy(buffer, (void*)buff + (t_write * s_size), w_sec * s_size);

		if(__usbstorage_WriteSectors(sector + t_write, w_sec, buffer) != 1)
		{
			dbgprintf("USB: Failed to write to USB device... Sector: %d Count: %d dst: %p\r\n", sector, count, buff);
			return RES_ERROR;
		}
	
		t_write += w_sec;
	}

	return RES_OK;
}

#endif

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
