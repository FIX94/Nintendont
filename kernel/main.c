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
#include "global.h"
#include "common.h"
#include "alloc.h"
#include "DI.h"
#include "RealDI.h"
#include "ES.h"
#include "SI.h"
#include "BT.h"
#include "lwbt/bte.h"
#include "Stream.h"
#include "HID.h"
#include "EXI.h"
#include "sock.h"
#include "GCNCard.h"
#include "debug.h"
#include "GCAM.h"
#include "TRI.h"
#include "Patch.h"

#include "diskio.h"
#include "usbstorage.h"
#include "SDI.h"
#include "ff_utf8.h"

//#define USE_OSREPORTDM 1

//#undef DEBUG
bool access_led = false;
u32 USBReadTimer = 0;
extern u32 s_size;
extern u32 s_cnt;

static FATFS *fatfs = NULL;
//this is just a single / as u16, easier to write in hex
static const WCHAR fatDevName[2] = { 0x002F, 0x0000 };

extern u32 SI_IRQ;
extern bool DI_IRQ, EXI_IRQ;
extern u32 WaitForRealDisc;
extern struct ipcmessage DI_CallbackMsg;
extern u32 DI_MessageQueue;
extern vu32 DisableSIPatch;
extern vu32 bbaEmuWanted;
extern char __bss_start, __bss_end;
extern char __di_stack_addr, __di_stack_size;

u32 virtentry = 0;
u32 drcAddress = 0;
u32 drcAddressAligned = 0;
bool isWiiVC = false;
bool wiiVCInternal = false;
int _main( int argc, char *argv[] )
{
	//BSS is in DATA section so IOS doesnt touch it, we need to manually clear it
	//dbgprintf("memset32(%08x, 0, %08x)\n", &__bss_start, &__bss_end - &__bss_start);
	memset32(&__bss_start, 0, &__bss_end - &__bss_start);
	sync_after_write(&__bss_start, &__bss_end - &__bss_start);

	//Important to do this as early as possible
	if(read32(0x20109740) == 0xE59F1004)
		virtentry = 0x20109740; //Address on Wii 
	else if(read32(0x2010999C) == 0xE59F1004)
		virtentry = 0x2010999C; //Address on WiiU

	//Use libwiidrc values to detect Wii VC
	sync_before_read((void*)0x12FFFFC0, 0x20);
	isWiiVC = read32(0x12FFFFC0);
	if(isWiiVC)
	{
		drcAddress = read32(0x12FFFFC4); //used in PADReadGC.c
		drcAddressAligned = ALIGN_BACKWARD(drcAddress,0x20);
	}

	s32 ret = 0;
	u32 DI_Thread = 0;

	BootStatus(0, 0, 0);

	if(!isWiiVC)
	{
		//Enable DVD Access
		write32(HW_DIFLAGS, read32(HW_DIFLAGS) & ~DI_DISABLEDVD);
	}

	thread_set_priority( 0, 0x50 );

	//Early HID for loader
	HIDInit();

	dbgprintf("Sending signal to loader\r\n");
	BootStatus(1, 0, 0);
	mdelay(10);

	//give power button to loader
	set32(HW_GPIO_ENABLE, GPIO_POWER);
	clear32(HW_GPIO_DIR, GPIO_POWER);
	set32(HW_GPIO_OWNER, GPIO_POWER);

	//Loader running, selects games
	while(1)
	{
		_ahbMemFlush(1);
		sync_before_read((void*)RESET_STATUS, 0x20);
		vu32 reset_status = read32(RESET_STATUS);
		if(reset_status != 0)
		{
			if(reset_status == 0x0DEA)
				break; //game selected
			else if(reset_status == 0x1DEA)
				goto WaitForExit;
			write32(RESET_STATUS, 0);
			sync_after_write((void*)RESET_STATUS, 0x20);
		}
		HIDUpdateRegisters(1);
		udelay(20);
		cc_ahbMemFlush(1);
	}
	//get time from loader
	InitCurrentTime();
	//get config from loader
	ConfigSyncBeforeRead();

	u32 UseUSB = ConfigGetConfig(NIN_CFG_USB);
	SetDiskFunctions(UseUSB);

	BootStatus(2, 0, 0);
	if(UseUSB)
	{
		ret = USBStorage_Startup();
		dbgprintf("USB:Drive size: %dMB SectorSize:%d\r\n", s_cnt / 1024 * s_size / 1024, s_size);
	}
	else
	{
		s_size = PAGE_SIZE512; //manually set s_size
		ret = SDHCInit();
	}
	if(ret != 1)
	{
		dbgprintf("Device Init failed:%d\r\n", ret );
		BootStatusError(-2, ret);
		mdelay(4000);
		Shutdown();
	}

	//Verification if we can read from disc
	if(memcmp(ConfigGetGamePath(), "di", 3) == 0)
	{
		if(isWiiVC) //will be inited later
			wiiVCInternal = true;
		else //will shutdown on fail
			RealDI_Init();
	}
	BootStatus(3, 0, 0);
	fatfs = (FATFS*)malloca( sizeof(FATFS), 32 );

	s32 res = f_mount( fatfs, fatDevName, 1 );
	if( res != FR_OK )
	{
		dbgprintf("ES:f_mount() failed:%d\r\n", res );
		BootStatusError(-3, res);
		mdelay(4000);
		Shutdown();
	}
	
	BootStatus(4, 0, 0);

	BootStatus(5, 0, 0);

	FIL fp;
	s32 fres = f_open_char(&fp, "/bladie", FA_READ|FA_OPEN_EXISTING);
	switch (fres)
	{
		case FR_OK:
			f_close(&fp);
			break;

		case FR_NO_PATH:
		case FR_NO_FILE:
			fres = FR_OK;
			break;

		default:
		case FR_DISK_ERR:
			BootStatusError(-5, fres);
			mdelay(4000);
			Shutdown();
			break;
	}

	if(!UseUSB) //Use FAT values for SD
		s_cnt = fatfs->n_fatent * fatfs->csize;

	BootStatus(6, s_size, s_cnt);

	BootStatus(7, s_size, s_cnt);
	ConfigInit();

	if (ConfigGetConfig(NIN_CFG_LOG))
		SDisInit = 1;  // Looks okay after threading fix
	dbgprintf("Game path: %s\r\n", ConfigGetGamePath());

	BootStatus(8, s_size, s_cnt);

	memset32((void*)RESET_STATUS, 0, 0x20);
	sync_after_write((void*)RESET_STATUS, 0x20);

	memset32((void*)0x13003100, 0, 0x30);
	sync_after_write((void*)0x13003100, 0x30);
	memset32((void*)0x13160000, 0, 0x20);
	sync_after_write((void*)0x13160000, 0x20);

	memset32((void*)0x13026500, 0, 0x100);
	sync_after_write((void*)0x13026500, 0x100);

	BootStatus(9, s_size, s_cnt);

	DIRegister();
	DI_Thread = do_thread_create(DIReadThread, ((u32*)&__di_stack_addr), ((u32)(&__di_stack_size)), 0x78);
	thread_continue(DI_Thread);

	DIinit(true);

	BootStatus(10, s_size, s_cnt);

	TRIInit();

	EXIInit();

	BootStatus(11, s_size, s_cnt);

	SIInit();
	StreamInit();

	PatchInit();

	SOCKInit();
//Tell PPC side we are ready!
	cc_ahbMemFlush(1);
	mdelay(1000);
	BootStatus(0xdeadbeef, s_size, s_cnt);
	mdelay(1000); //wait before hw flag changes
	dbgprintf("Kernel Start\r\n");
#ifdef USE_OSREPORTDM
	write32( 0x1860, 0xdeadbeef );	// Clear OSReport area
	sync_after_write((void*)0x1860, 0x20);
#endif
	u32 Now = read32(HW_TIMER);
	u32 PADTimer = Now;
	u32 DiscChangeTimer = Now;
	u32 ResetTimer = Now;
	u32 InterruptTimer = Now;
#ifdef PERFMON
	u32 loopCnt = 0;
	u32 loopPrintTimer = Now;
#endif
	USBReadTimer = Now;
	u32 Reset = 0;
	bool SaveCard = false;

	//enable ios led use
	access_led = ConfigGetConfig(NIN_CFG_LED);
	if(access_led)
	{
		set32(HW_GPIO_ENABLE, GPIO_SLOT_LED);
		clear32(HW_GPIO_DIR, GPIO_SLOT_LED);
		clear32(HW_GPIO_OWNER, GPIO_SLOT_LED);
	}

	set32(HW_GPIO_ENABLE, GPIO_SENSOR_BAR);
	clear32(HW_GPIO_DIR, GPIO_SENSOR_BAR);
	clear32(HW_GPIO_OWNER, GPIO_SENSOR_BAR);
	set32(HW_GPIO_OUT, GPIO_SENSOR_BAR);	//turn on sensor bar

	clear32(HW_GPIO_OWNER, GPIO_POWER); //take back power button

	write32( HW_PPCIRQMASK, (1<<30) ); //only allow IPC IRQ
	write32( HW_PPCIRQFLAG, read32(HW_PPCIRQFLAG) );

	//This bit seems to be different on japanese consoles
	u32 ori_ppcspeed = read32(HW_PPCSPEED);
	switch (BI2region)
	{
		case BI2_REGION_JAPAN:
		case BI2_REGION_SOUTH_KOREA:
		default:
			// JPN games.
			set32(HW_PPCSPEED, (1<<17));
			break;

		case BI2_REGION_USA:
		case BI2_REGION_PAL:
			// USA/PAL games.
			clear32(HW_PPCSPEED, (1<<17));
			break;
	}

	// Set the Wii U widescreen setting.
	u32 ori_widesetting = 0;
	if (IsWiiU())
	{
		ori_widesetting = read32(0xd8006a0);
		if( ConfigGetConfig(NIN_CFG_WIIU_WIDE) )
			write32(0xd8006a0, 0x30000004);
		else
			write32(0xd8006a0, 0x30000002);
		mask32(0xd8006a8, 0, 2);
	}

	while (1)
	{
		_ahbMemFlush(0);
#ifdef PERFMON
		loopCnt++;
		if(TimerDiffTicks(loopPrintTimer) > 1898437)
		{
			dbgprintf("%08i\r\n",loopCnt);
			loopPrintTimer = read32(HW_TIMER);
			loopCnt = 0;
		}
#endif
		//Does interrupts again if needed
		if(TimerDiffTicks(InterruptTimer) > 15820) //about 120 times a second
		{
			sync_before_read((void*)INT_BASE, 0x80);
			if((read32(RSW_INT) & 2) || (read32(DI_INT) & 4) || 
				(read32(SI_INT) & 8) || (read32(EXI_INT) & 0x10))
				write32(HW_IPC_ARMCTRL, 8); //throw irq
			InterruptTimer = read32(HW_TIMER);
		}
		#ifdef PATCHALL
		if (EXI_IRQ == true)
		{
			if(EXICheckTimer())
				EXIInterrupt();
		}
		#endif
		if (SI_IRQ != 0)
		{
			if ((TimerDiffTicks(PADTimer) > 7910) || (SI_IRQ & 0x2))	// about 240 times a second
			{
				SIInterrupt();
				PADTimer = read32(HW_TIMER);
			}
		}
		if(DI_IRQ == true)
		{
			if(DiscCheckAsync())
				DIInterrupt();
			else if(!bbaEmuWanted)
				udelay(200); //let the driver load data
		}
		else if(SaveCard == true) /* DI IRQ indicates we might read async, so dont write at the same time */
		{
			if(TimerDiffSeconds(Now) > 2) /* after 3 second earliest */
			{
				GCNCard_Save();
				SaveCard = false;
			}
		}
		else if(UseUSB && TimerDiffSeconds(USBReadTimer) > 149) /* Read random sector every 2 mins 30 secs */
		{
			DIFinishAsync(); //if something is still running
			DI_CallbackMsg.result = -1;
			sync_after_write(&DI_CallbackMsg, 0x20);
			IOS_IoctlAsync( DI_Handle, 2, NULL, 0, NULL, 0, DI_MessageQueue, &DI_CallbackMsg );
			DIFinishAsync();
			USBReadTimer = read32(HW_TIMER);
		}
		else /* No device I/O so make sure this stays updated */
			GetCurrentTime();
		udelay(20); //wait for other threads

		if( WaitForRealDisc == 1 )
		{
			if(RealDI_NewDisc())
			{
				DiscChangeTimer = read32(HW_TIMER);
				WaitForRealDisc = 2; //do another flush round, safety!
			}
		}
		else if( WaitForRealDisc == 2 )
		{
			if(TimerDiffSeconds(DiscChangeTimer))
			{
				//identify disc after flushing everything
				RealDI_Identify(false);
				//clear our fake regs again
				sync_before_read((void*)DI_BASE, 0x40);
				write32(DI_IMM, 0);
				write32(DI_COVER, 0);
				sync_after_write((void*)DI_BASE, 0x40);
				//mask and clear interrupts
				write32( DIP_STATUS, 0x54 );
				//disable cover irq which DIP enabled
				write32( DIP_COVER, 4 );
				DIInterrupt();
				WaitForRealDisc = 0;
			}
		}

		if ( DiscChangeIRQ == 1 )
		{
			DiscChangeTimer = read32(HW_TIMER);
			DiscChangeIRQ = 2;
		}
		else if ( DiscChangeIRQ == 2 )
		{
			if ( TimerDiffSeconds(DiscChangeTimer) > 2 )
			{
				DIInterrupt();
				DiscChangeIRQ = 0;
			}
		}
		_ahbMemFlush(1);
		DIUpdateRegisters();
		#ifdef PATCHALL
		EXIUpdateRegistersNEW();
		GCAMUpdateRegisters();
		BTUpdateRegisters();
		HIDUpdateRegisters(0);
		if(DisableSIPatch == 0) SIUpdateRegisters();
		#endif
		if(bbaEmuWanted)
		{
			SOCKUpdateRegisters();
			udelay(200);
		}
		StreamUpdateRegisters();
		CheckOSReport();
		if(GCNCard_CheckChanges())
		{
			Now = read32(HW_TIMER);
			SaveCard = true;
		}
		sync_before_read((void*)RESET_STATUS, 0x20);
		vu32 reset_status = read32(RESET_STATUS);
		if (reset_status == 0x1DEA)
		{
			dbgprintf("Game Exit\r\n");
			DIFinishAsync();
			break;
		}
		if (reset_status == 0x3DEA)
		{
			if (Reset == 0)
			{
				dbgprintf("Fake Reset IRQ\r\n");
				write32( RSW_INT, 0x2 ); // Reset irq
				sync_after_write( (void*)RSW_INT, 0x20 );
				write32(HW_IPC_ARMCTRL, 8); //throw irq
				Reset = 1;
			}
		}
		else if (Reset == 1)
		{
			write32( RSW_INT, 0x10000 ); // send pressed
			sync_after_write( (void*)RSW_INT, 0x20 );
			ResetTimer = read32(HW_TIMER);
			Reset = 2;
		}
		/* The cleanup is not connected to the button press */
		if (Reset == 2)
		{
			if (TimerDiffTicks(ResetTimer) > 949219) //free after half a second
			{
				write32( RSW_INT, 0 ); // done, clear
				sync_after_write( (void*)RSW_INT, 0x20 );
				Reset = 0;
			}
		}
		if(reset_status == 0x4DEA)
			PatchGame();
		if(reset_status == 0x5DEA)
		{
			SetIPL();
			PatchGame();
		}
		if(reset_status == 0x6DEA)
		{
			SetIPL_TRI();
			write32(RESET_STATUS, 0);
			sync_after_write((void*)RESET_STATUS, 0x20);
		}
		if(reset_status == 0x7DEA || (read32(HW_GPIO_IN) & GPIO_POWER))
		{
			DIFinishAsync();
			#ifdef PATCHALL
			BTE_Shutdown();
			#endif
			Shutdown();
		}
		#ifdef USE_OSREPORTDM
		sync_before_read( (void*)0x1860, 0x20 );
		if( read32(0x1860) != 0xdeadbeef )
		{
			if( read32(0x1860) != 0 )
			{
				dbgprintf(	(char*)(P2C(read32(0x1860))),
							(char*)(P2C(read32(0x1864))),
							(char*)(P2C(read32(0x1868))),
							(char*)(P2C(read32(0x186C))),
							(char*)(P2C(read32(0x1870))),
							(char*)(P2C(read32(0x1874)))
						);
			}
			write32(0x1860, 0xdeadbeef);
			sync_after_write( (void*)0x1860, 0x20 );
		}
		#endif
		cc_ahbMemFlush(1);
	}
	HIDClose();
	IOS_Close(DI_Handle); //close game
	thread_cancel(DI_Thread, 0);
	DIUnregister();

	if( ConfigGetConfig(NIN_CFG_MEMCARDEMU) )
		EXIShutdown();

	if (ConfigGetConfig(NIN_CFG_LOG))
		closeLog();

#ifdef PATCHALL
	BTE_Shutdown();
#endif

	//unmount FAT device
	f_mount(NULL, fatDevName, 1);
	free(fatfs);
	fatfs = NULL;

	if(UseUSB)
		USBStorage_Shutdown();
	else
		SDHCShutdown();

//make sure drive led is off before quitting
	if( access_led ) clear32(HW_GPIO_OUT, GPIO_SLOT_LED);

//make sure we set that back to the original
	write32(HW_PPCSPEED, ori_ppcspeed);

	if (IsWiiU())
	{
		write32(0xd8006a0, ori_widesetting);
		mask32(0xd8006a8, 0, 2);
	}
WaitForExit:
	/* Allow all IOS IRQs again */
	write32(HW_IPC_ARMCTRL, 0x36);
	/* Wii VC is unable to cleanly use ES */
	if(isWiiVC)
	{
		dbgprintf("Force reboot into WiiU Menu\n");
		WiiUResetToMenu();
	}
	else
	{
		dbgprintf("Kernel done, waiting for IOS Reload\n");
		write32(RESET_STATUS, 0);
		sync_after_write((void*)RESET_STATUS, 0x20);
	}
	while(1) mdelay(100);
	return 0;
}
