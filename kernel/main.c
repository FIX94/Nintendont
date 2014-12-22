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
#include "BT.h"
#include "lwbt/bte.h"
#include "Stream.h"
#include "HID.h"
#include "EXI.h"
#include "debug.h"
#include "GCAM.h"
#include "Patch.h"
#include "diskio.h"
#include "usbstorage.h"
#include "SDI.h"

#define DI_STACK_SIZE 0x400
//#undef DEBUG

u32 RealDiscCMD = 0;
u32 USBReadTimer = 0;
extern u32 s_size;
extern u32 s_cnt;

static FATFS *fatfs = NULL;
static const char *fatDevName = "/";

extern u32 SI_IRQ;
extern bool DI_IRQ, EXI_IRQ;
extern struct ipcmessage DI_CallbackMsg;
extern u32 DI_MessageQueue;

extern char __bss_start, __bss_end;
int _main( int argc, char *argv[] )
{
	//BSS is in DATA section so IOS doesnt touch it, we need to manually clear it
	//dbgprintf("memset32(%08x, 0, %08x)\n", &__bss_start, &__bss_end - &__bss_start);
	memset32(&__bss_start, 0, &__bss_end - &__bss_start);
	sync_after_write(&__bss_start, &__bss_end - &__bss_start);

	s32 ret = 0;
	u32 DI_Thread = 0;
	
	u8 MessageHeap[0x10];
	//u32 MessageQueue=0xFFFFFFFF;

	BootStatus(0, 0, 0);

	thread_set_priority( 0, 0x79 );	// do not remove this, this waits for FS to be ready!
	thread_set_priority( 0, 0x50 );
	thread_set_priority( 0, 0x79 );

	//MessageQueue = ES_Init( MessageHeap );
	ES_Init( MessageHeap );

	BootStatus(1, 0, 0);

	u32 UseUSB = ConfigGetConfig(NIN_CFG_USB);
	SetDiskFunctions(UseUSB);

	BootStatus(2, 0, 0);
	if(UseUSB)
	{
		u32 tmp = read32(HW_TIMER);
		while(TimerDiffSeconds(tmp) < 20)
		{
			if(USBStorage_Startup() && USBStorage_IsInserted()) //sets s_size and s_cnt
			{
				dbgprintf("USB:Drive size: %dMB SectorSize:%d\r\n", s_cnt / 1024 * s_size / 1024, s_size);
				ret = 1;
				break;
			}
			mdelay(50);
		}
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
		u32 Length = 0x20;
		RealDiscCMD = DIP_CMD_NORMAL;
		u8 *TmpBuf = ReadRealDisc(&Length, 0, false);
		if(read32((u32)TmpBuf+0x1C) != 0xC2339F3D)
		{
			Length = 0x800;
			RealDiscCMD = DIP_CMD_DVDR;
			TmpBuf = ReadRealDisc(&Length, 0, false);
			if(read32((u32)TmpBuf+0x1C) != 0xC2339F3D)
			{
				dbgprintf("No GC Disc!\r\n");
				BootStatusError(-2, -2);
				mdelay(4000);
				Shutdown();
			}
		}
		dbgprintf("DI:Reading real disc with command 0x%02X\r\n", RealDiscCMD);
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

	s32 fres = -1;
	FIL fp;
	while(fres != FR_OK)
	{
		fres = f_open(&fp, "/bladie", FA_READ|FA_OPEN_EXISTING);
		switch(fres)
		{
			case FR_OK:
				f_close(&fp);
			case FR_NO_PATH:
			case FR_NO_FILE:
			{
				fres = FR_OK;
			} break;
			default:
			case FR_DISK_ERR:
			{
				BootStatusError(-5, fres);
				mdelay(4000);
				Shutdown();
			} break;
		}
/*	//will nenver get here when timeout occures this loop is what is hung
		if(STATUS_ERROR == -7) { // FS check timed out on PPC side
			dbgprintf("FS check timed out\r\n");
			mdelay(3000);
			Shutdown();
		}
*/
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

	memset32((void*)0x13002800, 0, 0x30);
	sync_after_write((void*)0x13002800, 0x30);
	memset32((void*)0x13160000, 0, 0x20);
	sync_after_write((void*)0x13160000, 0x20);

	memset32((void*)0x13026500, 0, 0x100);
	sync_after_write((void*)0x13026500, 0x100);

	bool UseHID = ConfigGetConfig(NIN_CFG_HID);
	if( UseHID )
	{
		ret = HIDInit();
		if(ret < 0 )
		{
			dbgprintf("ES:HIDInit() failed\r\n" );
			BootStatusError(-8, ret);
			mdelay(4000);
			Shutdown();
		}
	}
	BootStatus(9, s_size, s_cnt);

	DIRegister();
	u32 *theDIStack = (u32*)malloca(DI_STACK_SIZE * sizeof(u32), 32); //yes, it seems like thats how it works
	DI_Thread = thread_create(DIReadThread, NULL, theDIStack, DI_STACK_SIZE, 0x78, 1); //simply because its a stack
	thread_continue(DI_Thread);

	DIinit(true);

	BootStatus(10, s_size, s_cnt);

	GCAMInit();

	EXIInit();

	ret = Check_Cheats();
	if(ret < 0 )
	{
		dbgprintf("Check_Cheats failed\r\n" );
		BootStatusError(-10, ret);
		mdelay(4000);
		Shutdown();
	}
	
	BootStatus(11, s_size, s_cnt);

	bool PatchSI = !ConfigGetConfig(NIN_CFG_NATIVE_SI);
	if (PatchSI)
		SIInit();
	StreamInit();

	PatchInit();

//This bit seems to be different on japanese consoles
	u32 ori_ppcspeed = read32(HW_PPCSPEED);
	if((ConfigGetGameID() & 0xFF) == 'J')
		set32(HW_PPCSPEED, (1<<17));
	else
		clear32(HW_PPCSPEED, (1<<17));

	//write32( 0x1860, 0xdeadbeef );	// Clear OSReport area

//Tell PPC side we are ready!
	cc_ahbMemFlush(1);
	mdelay(1000);
	BootStatus(0xdeadbeef, s_size, s_cnt);

	u32 Now = read32(HW_TIMER);
	u32 PADTimer = Now;
	u32 DiscChangeTimer = Now;
	u32 ResetTimer = Now;
	USBReadTimer = Now;
	u32 Reset = 0;
	bool SaveCard = false;
	if( ConfigGetConfig(NIN_CFG_LED) )
	{
		set32(HW_GPIO_ENABLE, GPIO_SLOT_LED);
		clear32(HW_GPIO_DIR, GPIO_SLOT_LED);
		clear32(HW_GPIO_OWNER, GPIO_SLOT_LED);
	}
	set32(HW_GPIO_ENABLE, GPIO_SENSOR_BAR);
	clear32(HW_GPIO_DIR, GPIO_SENSOR_BAR);
	clear32(HW_GPIO_OWNER, GPIO_SENSOR_BAR);
	set32(HW_GPIO_OUT, GPIO_SENSOR_BAR);	//turn on sensor bar

	EnableAHBProt(-1); //disable AHBPROT
	u32 ori_widesetting = read32(0xd8006a0);
	if(IsWiiU)
	{
		if( ConfigGetConfig(NIN_CFG_WIIU_WIDE) )
			write32(0xd8006a0, 0x30000004);
		else
			write32(0xd8006a0, 0x30000002);
		mask32(0xd8006a8, 0, 2);
	}
	while (1)
	{
		_ahbMemFlush(0);

		//Check this.  Purpose is to send another interrupt if wasn't processed
		/*if (((read32(0x14) != 0) || (read32(0x13026514) != 0))
			&& (read32(HW_ARMIRQFLAG) & (1 << 30)) == 0)
		{
			write32(HW_IPC_ARMCTRL, (1 << 0) | (1 << 4)); //throw irq
		}*/
		#ifdef PATCHALL
		if (EXI_IRQ == true)
		{
			if(EXICheckTimer())
				EXIInterrupt();
		}
		#endif
		if ((PatchSI) && (SI_IRQ != 0))
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
			else
				udelay(200); //let the driver load data
		}
		else if(SaveCard == true) /* DI IRQ indicates we might read async, so dont write at the same time */
		{
			if(TimerDiffSeconds(Now) > 2) /* after 3 second earliest */
			{
				EXISaveCard();
				SaveCard = false;
			}
		}
		else if(UseUSB && TimerDiffSeconds(USBReadTimer) > 9) /* Read random sector after about 10 seconds */
		{
			DI_CallbackMsg.result = -1;
			sync_after_write(&DI_CallbackMsg, 0x20);
			IOS_IoctlAsync( DI_Handle, 2, NULL, 0, NULL, 0, DI_MessageQueue, &DI_CallbackMsg );
			while(DI_CallbackMsg.result)
			{
				udelay(200); //wait for EHCI
				BTUpdateRegisters();
			}
			USBReadTimer = read32(HW_TIMER);
		}
		udelay(10); //wait for other threads

		//Baten Kaitos save hax
		/*if( read32(0) == 0x474B4245 )
		{
			if( read32( 0x0073E640 ) == 0xFFFFFFFF )
			{
				write32( 0x0073E640, 0 );
			}
		}*/

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
		if(UseHID) HIDUpdateRegisters();
		#endif
		StreamUpdateRegisters();
		CheckOSReport();
		if(EXICheckCard())
		{
			Now = read32(HW_TIMER);
			SaveCard = true;
		}
		if (PatchSI)
		{
			SIUpdateRegisters();
			if (read32(DI_SIMM) == 0x1DEA)
			{
				DIFinishAsync();
				break;
			}
			if (read32(DI_SIMM) == 0x3DEA)
			{
				if (Reset == 0)
				{
					dbgprintf("Fake Reset IRQ\n");
					write32(EXI2DATA, 0x2); // Reset irq
					write32(HW_IPC_ARMCTRL, (1 << 0) | (1 << 4)); //throw irq
					Reset = 1;
				}
			}
			else if (Reset == 1)
			{
				write32(EXI2DATA, 0x10000); // send pressed
				ResetTimer = read32(HW_TIMER);
				Reset = 2;
			}
			/* The cleanup is not connected to the button press */
			if (Reset == 2)
			{
				if (TimerDiffTicks(ResetTimer) > 949219) //free after half a second
				{
					write32(EXI2DATA, 0); // done, clear
					write32(DI_SIMM, 0);
					sync_after_write( (void*)DI_BASE, 0x60 );
					Reset = 0;
				}
			}
		}
		if(read32(DI_SIMM) == 0x4DEA)
			PatchGame();
		CheckPatchPrs();
		if(read32(HW_GPIO_IN) & GPIO_POWER)
		{
			DIFinishAsync();
			#ifdef PATCHALL
			BTE_Shutdown();
			#endif
			Shutdown();
		}
		//sync_before_read( (void*)0x1860, 0x20 );
		//if( read32(0x1860) != 0xdeadbeef )
		//{
		//	if( read32(0x1860) != 0 )
		//	{
		//		dbgprintf(	(char*)(P2C(read32(0x1860))),
		//					(char*)(P2C(read32(0x1864))),
		//					(char*)(P2C(read32(0x1868))),
		//					(char*)(P2C(read32(0x186C))),
		//					(char*)(P2C(read32(0x1870))),
		//					(char*)(P2C(read32(0x1874)))
		//				);
		//	}
		//	write32(0x1860, 0xdeadbeef);
		//	sync_after_write( (void*)0x1860, 0x20 );
		//}
		cc_ahbMemFlush(1);
	}
	if( UseHID )
		HIDClose();
	IOS_Close(DI_Handle); //close game
	thread_cancel(DI_Thread, 0);
	DIUnregister();

	write32( DI_SIMM, 0 );
	sync_after_write( (void*)DI_BASE, 0x60 );
	/* reset time */
	while(1)
	{
		sync_before_read( (void*)DI_BASE, 0x60 );
		if(read32(DI_SIMM) == 0x2DEA)
			break;
		wait_for_ppc(1);
	}

	if( ConfigGetConfig(NIN_CFG_LED) )
		clear32(HW_GPIO_OUT, GPIO_SLOT_LED);

	if( ConfigGetConfig(NIN_CFG_MEMCARDEMU) )
		EXIShutdown();

	if (ConfigGetConfig(NIN_CFG_LOG))
		closeLog();

#ifdef PATCHALL
	BTE_Shutdown();
#endif

//unmount FAT device
	free(fatfs);
	fatfs = NULL;
	f_mount(NULL, fatDevName, 1);

	if(UseUSB)
		USBStorage_Shutdown();
	else
		SDHCShutdown();

//make sure we set that back to the original
	write32(HW_PPCSPEED, ori_ppcspeed);

	if(IsWiiU)
	{
		write32(0xd8006a0, ori_widesetting);
		mask32(0xd8006a8, 0, 2);
	}

	IOSBoot((char*)0x13003020, 0, read32(0x13003000));
	return 0;
}
