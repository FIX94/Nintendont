/*

Nintendont (Kernel) - Playing Gamecubes in Wii mode on a Wii U

Copyright (C) 2013  crediar

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/
#include "string.h"
#include "syscalls.h"
#include "global.h"
#include "ipc.h"
#include "common.h"
#include "alloc.h"
#include "DI.h"
#include "ES.h"
#include "SI.h"
#include "StreamADPCM.h"
#include "HID.h"
#include "EXI.h"
#include "debug.h"
#ifdef NINTENDONT_USB
#include "usb.h"
#else
#include "SDI.h"
#endif

int verbose = 0;
u32 base_offset=0;
void *queuespace=NULL;
int queueid = 0;
int heapid=0;
int FFSHandle=0;
u32 FSUSB=0;
FIL GameFile;
void *reset = (void*)0x1200300C;
//#undef DEBUG

extern u32 s_size;
extern u32 s_cnt;

FATFS *fatfs;
extern u32 HIDHandle;
extern bool SI_IRQ;
//u32 Loopmode=0;
int _main( int argc, char *argv[] )
{
	s32 ret = 0;
	
	u8 MessageHeap[0x10];
	//u32 MessageQueue=0xFFFFFFFF;

	BootStatus(0, 0, 0);

	thread_set_priority( 0, 0x79 );	// do not remove this, this waits for FS to be ready!
	thread_set_priority( 0, 0x50 );
	thread_set_priority( 0, 0x79 );

	#if 0

	#ifdef DEBUG
	u32 v = read32(0x3140);
	dbgprintf("Nintendont IOS%d v%d.%d\r\n", v >> 16, (v >> 8) & 0xff, v & 0xff);

	dbgprintf("Built   : %s %s\r\n", __DATE__, __TIME__ );
	dbgprintf("Version : %d.%d\r\n", NIN_VERSION>>16, NIN_VERSION&0xFFFF );
	#endif

	#endif

	//MessageQueue = ES_Init( MessageHeap );
	ES_Init( MessageHeap );

	BootStatus(1, 0, 0);

#ifndef NINTENDONT_USB
	BootStatus(2, 0, 0);
	ret = SDHCInit();
	if(!ret)
	{
		dbgprintf("SD:SDHCInit() failed:%d\r\n", ret );
		BootStatusError(-2, ret);
		mdelay(2000);
		Shutdown();
	}
#endif
	BootStatus(3, 0, 0);
	fatfs = (FATFS*)malloca( sizeof(FATFS), 32 );

	s32 res = f_mount( 0, fatfs );
	if( res != FR_OK )
	{
		dbgprintf("ES:f_mount() failed:%d\r\n", res );
		BootStatusError(-3, res);
		mdelay(2000);
		Shutdown();
	}
	
	BootStatus(4, 0, 0);
#ifdef NINTENDONT_USB
	BootStatus(5, 0, 0);
	
	int MountFail = 0;
	s32 fres = -1; 
	while(fres != FR_OK)
	{
		fres = f_open(&GameFile, "/bladie", FA_READ|FA_OPEN_EXISTING);
		switch(fres)
		{
			case FR_OK:
				f_close(&GameFile);
			case FR_NO_PATH:
			case FR_NO_FILE:
			{
				fres = FR_OK;
			} break;
			default:
			case FR_DISK_ERR:
			{
				f_mount(0, 0);		//unmount drive todo: retry could never work
				MountFail++;
				if(MountFail == 10)
				{
					BootStatusError(-5, fres);
					mdelay(2000);
					Shutdown();
				}
				mdelay(5);
			} break;
		}
	}

	BootStatus(6, s_size, s_cnt);
	s32 r = LoadModules(55);
	//dbgprintf("ES:ES_LoadModules(%d):%d\r\n", 55, r );
	if( r < 0 )
	{
		BootStatusError(-6, r);
		mdelay(2000);
		Shutdown();
	}
#endif
	BootStatus(7, s_size, s_cnt);
	ConfigInit();
	
	BootStatus(8, s_size, s_cnt);

	SDisInit = 1;

	thread_set_priority( 0, 127 );

	u32 HID_Thread = 0;
	bool UseHID = ConfigGetConfig(NIN_CFG_HID);
	if( UseHID )
	{
		ret = HIDInit();
		if(ret < 0 )
		{
			dbgprintf("ES:HIDInit() failed\r\n" );
			BootStatusError(-8, ret);
			mdelay(2000);
			Shutdown();
		}
		write32(0x12003004, 0);
		sync_after_write((void*)0x12003004, 0x20);

		memset32((void*)0x12003420, 0, 0x1BE0);
		sync_after_write((void*)0x12003420, 0x1BE0);
		HID_Thread = thread_create(HID_Run, NULL, (u32*)0x12003420, 0x1BE0, 0x50, 1);
		thread_continue(HID_Thread);
		mdelay(500);
	}
	BootStatus(9, s_size, s_cnt);

	DIinit();
	BootStatus(10, s_size, s_cnt);

	EXIInit();
	BootStatus(11, s_size, s_cnt);

//fixes issues in some japanese games
	if((ConfigGetGameID() & 0xFF) == 'J')
		write32(HW_PPCSPEED, 0x2A9E0);

//Tell PPC side we are ready!
	mdelay(1000);
	BootStatus(0xdeadbeef, s_size, s_cnt);
/*
	write32( HW_PPCIRQFLAG, read32(HW_PPCIRQFLAG) );
	write32( HW_ARMIRQFLAG, read32(HW_ARMIRQFLAG) );

	set32( HW_PPCIRQMASK, (1<<31) );
	set32( HW_IPC_PPCCTRL, 0x30 );
*/
	SIInit();
	u32 Now = read32(HW_TIMER);
	u32 PADTimer = Now;

	bool SaveCard = false;

	//thread_set_priority( 0, 127 );
	cc_ahbMemFlush(1);
	write32(0xd8006a0, 0x30000004), mask32(0xd8006a8, 0, 2);
	while (1)
	{
		_ahbMemFlush(0);

		if(SI_IRQ == true)
		{
			if((read32(HW_TIMER) - PADTimer) >= 65000)	// about 29 times a second
			{
				SIInterrupt();
				PADTimer = read32(HW_TIMER);
			}
		}

		//Baten Kaitos save hax
		if( read32(0) == 0x474B4245 )
		{
			if( read32( 0x0073E640 ) == 0xFFFFFFFF )
			{
				write32( 0x0073E640, 0 );
			}
		}

		if( Streaming )
		{
			if( (read32(HW_TIMER) * 19 / 10) - StreamTimer >= 5000000 )
			{
			//	dbgprintf(".");
				StreamOffset += 64*1024;

				if( StreamOffset >= StreamSize )
				{
					StreamOffset = StreamSize;
					Streaming = 0;
				}
				StreamTimer = read32(HW_TIMER) * 19 / 10;
			}
		}

		if( DiscChangeIRQ )
		{
			if( read32(HW_TIMER) * 128 / 243000000 > 2 )
			{
				//dbgprintf("DIP:IRQ mon!\r\n");

				while( read32(DI_SCONTROL) & 1 )
					clear32( DI_SCONTROL, 1 );

				set32( DI_SSTATUS, 0x3A );

				write32( 0x0D806000, 0x3E );
				write32( 0x0D806008, 0xE0000000 );
				write32( 0x0D806020, 0 );
				write32( 0x0D80601C, 1 );

				DiscChangeIRQ = 0;
			}
		}
		_ahbMemFlush(1);
		DIUpdateRegisters();
		EXIUpdateRegistersNEW();
		SIUpdateRegisters();
		if(EXICheckCard() && !SaveCard)
		{
			Now = read32(HW_TIMER);
			SaveCard = true;
		}
		if(SaveCard == true)
		{
			if((read32(HW_TIMER) - Now) * 1024 / 243000000 > 5)
			{
				EXISaveCard();
				SaveCard = false;
			}
		}
		sync_before_read(reset, 0x20);
		if(read32((u32)reset) == 1)
		{
			write32((u32)reset, 0);
			sync_after_write(reset, 0x20);
			//dbgprintf("loop quit\r\n");
			break;
		}
		cc_ahbMemFlush(1);
	}
	if( UseHID )
	{
		/* we're done reading inputs */
		thread_cancel(HID_Thread, 0);
	}
	if( ConfigGetConfig(NIN_CFG_MEMCARDEMU) )
		EXIShutdown();
	/* reset time */
	while(1)
	{
		_ahbMemFlush(0);
		sync_before_read(reset, 0x20);
		if(read32((u32)reset) == 2)
			break;
		cc_ahbMemFlush(1);
	}
	IOSBoot((char*)0x12003020, 0, read32(0x12003000));
	return 0;
}
