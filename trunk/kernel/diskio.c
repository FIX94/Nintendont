#include "diskio.h"
#include "string.h"
#include "debug.h"
#include "config.h"

#ifndef NINTENDONT_USB
#include "SDI.h"
#include "syscalls.h"
#else
#include "ehci.h"
#include "alloc.h"
#include "common.h"
#endif

#define GPIO_SLOT_LED	(1<<5)

u32 s_size;
u32 s_cnt;
u32 max_sec = 4;
u8 *buffer = (u8*)NULL;

#ifndef NINTENDONT_USB

DSTATUS disk_initialize( BYTE drv, WORD *ss )
{
	return 0;
}
DSTATUS disk_status( BYTE drv )
{
	return 0;
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


int tiny_ehci_init(void);

DSTATUS disk_initialize(BYTE drv, WORD *ss)
{
	s32 r = 1;	
	
	udelay(50000);
	tiny_ehci_init();
	
	while(1)
	{
		dbgprintf("USB:Discovering EHCI devices...\r\n");
		while(ehci_discover() == -ENODEV)
			udelay(4000);
	
		r = USBStorage_Init();
		
		if(r == 0)
			break;
	}		
	
	s_cnt = USBStorage_Get_Capacity(&s_size);
	
	*ss = s_size;	
	
	max_sec = 0x8000 / s_size;
	
	buffer = (u8*)malloca(max_sec * s_size, 32);
	
	dbgprintf("USB:Drive size: %dMB SectorSize:%d\r\n", s_cnt / 1024 * s_size / 1024, s_size);

	return r;
}

DSTATUS disk_status(BYTE drv)
{
	(void)drv;
	return 0;
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

		if(USBStorage_Read_Sectors(sector+t_read, r_sec, buffer) != 1)
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

		if(USBStorage_Write_Sectors(sector + t_write, w_sec, buffer) != 1)
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
