/*

Nintendont (Loader) - Playing Gamecubes in Wii mode on a Wii U

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
#include <gccore.h>
#include <sys/param.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/lwp_threads.h>
#include <wiiuse/wpad.h>
#include <wiidrc/wiidrc.h>
#include <wupc/wupc.h>
#include <di/di.h>
#include <unistd.h>
#include <locale.h>

#include "exi.h"
#include "dip.h"
#include "global.h"
#include "font.h"
#include "FPad.h"
#include "menu.h"
#include "MemCard.h"
#include "MemCardPro.h"
#include "Patches.h"
#include "kernel_zip.h"
#include "kernelboot_bin.h"
#include "multidol_ldr_bin.h"
#include "stub_bin.h"
#include "IOSInterface_bin.h"
#include "titles.h"
#include "ipl.h"
#include "HID.h"
#include "TRI.h"
#include "Config.h"
#include "wdvd.h"

#include "ff_utf8.h"
#include "diskio.h"
// from diskio.c
extern DISC_INTERFACE *driver[_VOLUMES];

extern void __exception_setreload(int t);
extern void __SYS_ReadROM(void *buf,u32 len,u32 offset);
extern u32 __SYS_GetRTC(u32 *gctime);

extern syssram* __SYS_LockSram();
extern u32 __SYS_UnlockSram(u32 write);
extern u32 __SYS_SyncSram(void);

#define STATUS			((void*)0x90004100)
#define STATUS_LOADING	(*(volatile unsigned int*)(0x90004100))
#define STATUS_SECTOR	(*(volatile unsigned int*)(0x90004100 + 8))
#define STATUS_DRIVE	(*(float*)(0x9000410C))
#define STATUS_GB_MB	(*(volatile unsigned int*)(0x90004100 + 16))
#define STATUS_ERROR	(*(volatile unsigned int*)(0x90004100 + 20))

#define HW_DIFLAGS		0xD800180
#define MEM_PROT		0xD8B420A

#define IOCTL_ExecSuspendScheduler	1

static GXRModeObj *vmode = NULL;

static unsigned char ESBootPatch[] =
{
    0x48, 0x03, 0x49, 0x04, 0x47, 0x78, 0x46, 0xC0, 0xE6, 0x00, 0x08, 0x70, 0xE1, 0x2F, 0xFF, 0x1E, 
    0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x0D, 0x25,
};
/*static const unsigned char AHBAccessPattern[] =
{
	0x68, 0x5B, 0x22, 0xEC, 0x00, 0x52, 0x18, 0x9B, 0x68, 0x1B, 0x46, 0x98, 0x07, 0xDB,
};
static const unsigned char AHBAccessPatch[] =
{
	0x68, 0x5B, 0x22, 0xEC, 0x00, 0x52, 0x18, 0x9B, 0x23, 0x01, 0x46, 0x98, 0x07, 0xDB,
};*/
static const unsigned char FSAccessPattern[] =
{
    0x9B, 0x05, 0x40, 0x03, 0x99, 0x05, 0x42, 0x8B, 
};
static const unsigned char FSAccessPatch[] =
{
    0x9B, 0x05, 0x40, 0x03, 0x1C, 0x0B, 0x42, 0x8B, 
};

// Forbid the use of MEM2 through malloc
u32 MALLOC_MEM2 = 0;

s32 __IOS_LoadStartupIOS(void)
{
	int res;

	res = __ES_Init();
	if(res < 0)
		return res;

	return 0;
}

// Storage devices. (defined in global.c)
// 0 == SD, 1 == USB
extern FATFS *devices[2];

vu32 KernelLoaded = 0;
u32 entrypoint = 0;
char launch_dir[MAXPATHLEN] = {0};
extern void __exception_closeall();
static u8 loader_stub[0x1C00]; //save internally to prevent overwriting
static ioctlv IOCTL_Buf[2] ALIGNED(32);
static const char ARGSBOOT_STR[9] ALIGNED(0x10) = {'a','r','g','s','b','o','o','t','\0'}; //makes it easier to go through the file
static const char NIN_BUILD_STRING[] ALIGNED(32) = NIN_VERSION_STRING; // Version detection string used by nintendont launchers "$$Version:x.xxx"

bool isWiiVC = false;
bool wiiVCInternal = false;

/**
 * Update meta.xml.
 */
static void updateMetaXml(void)
{
	char filepath[MAXPATHLEN];
	bool dir_argument_exists = strlen(launch_dir);
	
	snprintf(filepath, sizeof(filepath), "%smeta.xml",
		dir_argument_exists ? launch_dir : "/apps/Nintendont/");

	if (!dir_argument_exists) {
		gprintf("Creating new directory\r\n");
		f_mkdir_char("/apps");
		f_mkdir_char("/apps/Nintendont");
	}

	char new_meta[1024];
	int len = snprintf(new_meta, sizeof(new_meta),
		META_XML "\r\n<app version=\"1\">\r\n"
		"\t<name>" META_NAME "</name>\r\n"
		"\t<coder>" META_AUTHOR "</coder>\r\n"
		"\t<version>%d.%d%s</version>\r\n"
		"\t<release_date>20160710000000</release_date>\r\n"
		"\t<short_description>" META_SHORT "</short_description>\r\n"
		"\t<long_description>" META_LONG1 "\r\n\r\n" META_LONG2 "</long_description>\r\n"
		"\t<no_ios_reload/>\r\n"
		"\t<ahb_access/>\r\n"
		"</app>\r\n",
		NIN_VERSION >> 16, NIN_VERSION & 0xFFFF,
#ifdef NIN_SPECIAL_VERSION
		NIN_SPECIAL_VERSION
#else
		""
#endif
  			);
	if (len > sizeof(new_meta))
		len = sizeof(new_meta);

	// Check if the file already exists.
	FIL meta;
	if (f_open_char(&meta, filepath, FA_READ|FA_OPEN_EXISTING) == FR_OK)
	{
		// File exists. If it's the same as the new meta.xml,
		// don't bother rewriting it.
		char orig_meta[1024];
		if (len == meta.obj.objsize)
		{
			// File is the same length.
			UINT read;
			f_read(&meta, orig_meta, len, &read);
			if (read == (UINT)len &&
			    !strncmp(orig_meta, new_meta, len))
			{
				// File is identical.
				// Don't rewrite it.
				f_close(&meta);
				return;
			}
		}
		f_close(&meta);
	}

	// File does not exist, or file is not identical.
	// Write the new meta.xml.
	if (f_open_char(&meta, filepath, FA_WRITE|FA_CREATE_ALWAYS) == FR_OK)
	{
		// Reserve space in the file.
		if (f_size(&meta) < len) {
			f_expand(&meta, len, 1);
		}

		// Write the new meta.xml.
		UINT wrote;
		f_write(&meta, new_meta, len, &wrote);
		f_close(&meta);
		FlushDevices();
	}
}

static const WCHAR *primaryDevice;
void changeToDefaultDrive()
{
	f_chdrive(primaryDevice);
	f_chdir_char("/");
}

/**
 * Get multi-game and region code information.
 * @param CurDICMD	[in] DI command. (0 == disc image, DIP_CMD_NORMAL == GameCube disc, DIP_CMD_DVDR == DVD-R)
 * @param ISOShift	[out,opt] ISO Shift. (34-bit rshifted byte offset)
 * @param BI2region	[out,opt] bi2.bin region code.
 * @return 0 on success; non-zero on error.
 */
static u32 CheckForMultiGameAndRegion(unsigned int CurDICMD, u32 *ISOShift, u32 *BI2region)
{
	char GamePath[260];

	// Re-read the disc header to get the full ID6.
	u8 *MultiHdr = memalign(32, 0x800);

	FIL f;
	UINT read;
	FRESULT fres = FR_DISK_ERR;

	if(CurDICMD)
		ReadRealDisc(MultiHdr, 0, 0x800, CurDICMD);
	else if (IsSupportedFileExt(ncfg->GamePath) || wiiVCInternal)
	{
		if(wiiVCInternal)
		{
			if(WDVD_FST_OpenDisc(0) != 0)
			{
				// Error opening the file.
				free(MultiHdr);
				return 1;
			}
		}
		else
		{
			snprintf(GamePath, sizeof(GamePath), "%s:%s", GetRootDevice(), ncfg->GamePath);
			fres = f_open_char(&f, GamePath, FA_READ|FA_OPEN_EXISTING);
			if (fres != FR_OK)
			{
				// Error opening the file.
				free(MultiHdr);
				return 1;
			}
		}
		if(wiiVCInternal)
			read = WDVD_FST_Read(MultiHdr, 0x800);
		else
			f_read(&f, MultiHdr, 0x800, &read);
		if (read != 0x800)
		{
			// Error reading from the file.
			if(wiiVCInternal)
				WDVD_FST_Close();
			else
				f_close(&f);
			free(MultiHdr);
			return 2;
		}

		// Check for CISO magic with 2 MB block size.
		// NOTE: CISO block size is little-endian.
		static const uint8_t CISO_MAGIC[8] = {'C','I','S','O',0x00,0x00,0x20,0x00};
		if (!memcmp(MultiHdr, CISO_MAGIC, sizeof(CISO_MAGIC)) && !IsGCGame(MultiHdr))
		{
			// CISO magic is present, and GCN magic isn't.
			// This is most likely a CISO image.

			// CISO+MultiGame is not supported, so read the
			// BI2.bin region code if requested and then return.
			int ret = 0;
			if (ISOShift)
				*ISOShift = 0;
			if (BI2region)
			{
				if(wiiVCInternal)
				{
					WDVD_FST_LSeek(0x8458);
					read = WDVD_FST_Read(wdvdTmpBuf, sizeof(*BI2region));
					memcpy(&BI2region, wdvdTmpBuf, sizeof(*BI2region));
				}
				else
				{
					f_lseek(&f, 0x8458);
					f_read(&f, BI2region, sizeof(*BI2region), &read);
				}
				if (read != sizeof(*BI2region))
				{
					// Error reading from the file.
					ret = 3;
				}
			}
			if(wiiVCInternal)
				WDVD_FST_Close();
			else
				f_close(&f);
			free(MultiHdr);
			return ret;
		}
	}
	else
	{
		// Extracted FST format.
		// Multi-game isn't supported.
		if (ISOShift)
			*ISOShift = 0;
		if (!BI2region)
		{
			free(MultiHdr);
			return 0;
		}

		// Get the bi2.bin region code.
		snprintf(GamePath, sizeof(GamePath), "%s:%ssys/bi2.bin", GetRootDevice(), ncfg->GamePath);
		fres = f_open_char(&f, GamePath, FA_READ|FA_OPEN_EXISTING);
		if (fres != FR_OK)
		{
			// Error opening bi2.bin.
			free(MultiHdr);
			return 4;
		}

		// bi2.bin is normally 8 KB, but we only need
		// the first 48 bytes.
		f_read(&f, MultiHdr, 48, &read);
		f_close(&f);
		if (read != 48)
		{
			// Could not read bi2.bin.
			free(MultiHdr);
			return 5;
		}

		// BI2.bin is at 0x440.
		// Region code is at 0x458. (0x18 within BI2.bin.)
		*BI2region = *(u32*)(&MultiHdr[0x18]);
		free(MultiHdr);
		return 0;
	}

	if (!IsMultiGameDisc((const char*)MultiHdr))
	{
		// Not a multi-game disc.
		if (!CurDICMD)
		{
			// Close the disc image file.
			if(wiiVCInternal)
				WDVD_FST_Close();
			else
				f_close(&f);
		}

		if (BI2region)
		{
			// BI2.bin is at 0x440.
			// Region code is at 0x458.
			*BI2region = *(u32*)(&MultiHdr[0x458]);
		}

		free(MultiHdr);
		return 0;
	}

	// Up to 15 games are supported.
	// In theory, the format supports up to 48 games, but
	// you'll run into the DVD size limit way before you
	// ever reach that limit.
	u32 i = 0;
	u32 gamecount = 0;
	u32 Offsets[15]; // 34-bit, rshifted by 2
	u32 BI2region_codes[15];
	gameinfo gi[15];

	// Games must be aligned to 4-byte boundaries, since
	// we're using 34-bit rsh2 (Wii) offsets.
	u8 gameIsUnaligned[15];
	memset(gameIsUnaligned, 0, sizeof(gameIsUnaligned));

	u8 *GameHdr = memalign(32, 0x800);
	// GCOPDV(D9) uses Wii-style 34-bit shifted addresses.
	// FIXME: Needs 64-bit offsets.
	const u32 *hdr32 = (const u32*)MultiHdr;
	bool IsShifted = (hdr32[1] == 0x44564439);
	for (i = 0x10; i < 0x40 && gamecount < 15; i++)
	{
		const u32 TmpOffset = hdr32[i];
		if (TmpOffset > 0)
		{
			u64 RealOffset;
			if (IsShifted)
			{
				// Disc uses 34-bit shifted offsets.
				Offsets[gamecount] = TmpOffset;
				RealOffset = (u64)TmpOffset << 2;
			}
			else
			{
				// Disc uses 32-bit unshifted offsets.
				// If the value isn't a multiple of 4, it's unusable.
				// TODO: Fix this, or will this "never" happen?
				gameIsUnaligned[gamecount] = !!(TmpOffset & 3);
				Offsets[gamecount] = TmpOffset >> 2;
				RealOffset = TmpOffset;
			}

			if(CurDICMD)
				ReadRealDisc(GameHdr, RealOffset, 0x800, CurDICMD);
			else
			{
				if(wiiVCInternal)
				{
					WDVD_FST_LSeek(RealOffset);
					read = WDVD_FST_Read(GameHdr, 0x800);
				}
				else
				{
					f_lseek(&f, RealOffset);
					f_read(&f, GameHdr, 0x800, &read);
				}
			}

			// Make sure the title in the header is NULL terminated.
			GameHdr[0x20+65] = 0;

			// BI2.bin is at 0x440.
			// Region code is at 0x458.
			BI2region_codes[gamecount] = *(u32*)(&GameHdr[0x458]);

			// TODO: titles.txt support?
			memcpy(gi[gamecount].ID, GameHdr, 6);
			gi[gamecount].Revision = GameHdr[0x07];
			gi[gamecount].Flags = GIFLAG_NAME_ALLOC;
			gi[gamecount].Name = strdup((char*)&GameHdr[0x20]);
			gi[gamecount].Path = NULL;
			gamecount++;
		}
	}

	free(GameHdr);
	free(MultiHdr);
	if (!CurDICMD)
	{
		if(wiiVCInternal)
			WDVD_FST_Close();
		else
			f_close(&f);
	}

	// TODO: Share code with menu.c.
	bool redraw = true;
	ClearScreen();
	u32 PosX = 0;
	u32 UpHeld = 0, DownHeld = 0;
	while (true)
	{
		VIDEO_WaitVSync();
		FPAD_Update();
		if( FPAD_OK(0) )
		{
			// TODO: Fix support for unaligned games?
			if (!gameIsUnaligned[PosX])
				break;
		}

		if( FPAD_Down(1) )
		{
			if(DownHeld == 0 || DownHeld > 10)
			{
				PosX++;
				if(PosX == gamecount) PosX = 0;
				redraw = true;
			}
			DownHeld++;
		}
		else
		{
			DownHeld = 0;
		}

		if( FPAD_Up(1) )
		{
			if(UpHeld == 0 || UpHeld > 10)
			{
				if(PosX == 0) PosX = gamecount;
				PosX--;
				redraw = true;
			}
			UpHeld++;
		}
		else
		{
			UpHeld = 0;
		}

		// TODO: Home = Go Back?

		if( redraw )
		{
			PrintInfo();
			PrintButtonActions(NULL, "Select", NULL, NULL);
			static const int subheader_x = (640 - (40*10)) / 2;
			PrintFormat(DEFAULT_SIZE, BLACK, subheader_x, MENU_POS_Y + 20*3,
				    "Select a game from this multi-game disc:");
			for (i = 0; i < gamecount; ++i)
			{
				const u32 color = gameIsUnaligned[i] ? MAROON : BLACK;
				PrintFormat(DEFAULT_SIZE, color, MENU_POS_X, MENU_POS_Y + 20*4 + i * 20, "%50.50s [%.6s]%s", 
					    gi[i].Name, gi[i].ID, i == PosX ? ARROW_LEFT : " " );
			}
			GRRLIB_Render();
			Screenshot();
			ClearScreen();
			redraw = false;
		}
	}

	// Free the allocated names.
	for (i = 0; i < gamecount; ++i)
	{
		if (gi[i].Flags & GIFLAG_NAME_ALLOC)
			free(gi[i].Name);
	}

	// Set the ISOShift and BI2region values.
	if (ISOShift)
	{
		*ISOShift = Offsets[PosX];
	}
	if (BI2region)
	{
		*BI2region = BI2region_codes[PosX];
	}

	// Save the Game ID.
	memcpy(&ncfg->GameID, gi[PosX].ID, 4);
	return 0;
}

static char dev_es[] ATTRIBUTE_ALIGN(32) = "/dev/es";

extern vu32 FoundVersion;
extern void _jmp813();
int main(int argc, char **argv)
{
	// Exit after 10 seconds if there is an error
	__exception_setreload(10);
//	u64 timeout = 0;
	CheckForGecko();
	DCInvalidateRange(loader_stub, 0x1800);
	memcpy(loader_stub, (void*)0x80001800, 0x1800);
	RAMInit();
	//tell devkitPPC r29 that we use UTF-8
	setlocale(LC_ALL,"C.UTF-8");

	memset((void*)ncfg, 0, sizeof(NIN_CFG));
	bool argsboot = false;
	if(argc > 1) //every 0x00 gets counted as one arg so just make sure its more than the path and copy
	{
		memcpy(ncfg, argv[1], sizeof(NIN_CFG));
		UpdateNinCFG(); //support for old versions with this
		if(ncfg->Magicbytes == 0x01070CF6 && ncfg->Version == NIN_CFG_VERSION && ncfg->MaxPads <= NIN_CFG_MAXPAD)
		{
			if(ncfg->Config & NIN_CFG_AUTO_BOOT)
			{	//do NOT remove, this can be used to see if nintendont knows args
				gprintf(ARGSBOOT_STR);
				argsboot = true;
			}
		}
	}

	//Meh, doesnt do anything anymore anyways
	//STM_RegisterEventHandler(HandleSTMEvent);

	Initialise(argsboot);

	//for BT.c
	CONF_GetPadDevices((conf_pads*)0x932C0000);
	DCFlushRange((void*)0x932C0000, sizeof(conf_pads));
	*(vu32*)0x932C0490 = CONF_GetIRSensitivity();
	*(vu32*)0x932C0494 = CONF_GetSensorBarPosition();
	DCFlushRange((void*)0x932C0490, 8);

	WiiDRC_Init();
	isWiiVC = WiiDRC_Inited();
	if(isWiiVC)
	{
		//Clear out stubs to make it (hopefully) reboot properly
		memset(loader_stub, 0, 0x1800);
		memset((void*)0x80001800, 0, 0x1800);
		DCStoreRange((void*)0x80001800, 0x1800);
	}

	s32 fd;

	/* Wii VC fw.img is pre-patched but Wii/vWii isnt, so we
		still have to reload IOS on those with a patched kernel */
	if(!isWiiVC)
	{
		// Preparing IOS58 Kernel...
		if(argsboot == false)
			ShowMessageScreen("Preparing IOS58 Kernel...");

		u32 u;
		//Disables MEMPROT for patches
		write16(MEM_PROT, 0);
		//Patches FS access
		for( u = 0x93A00000; u < 0x94000000; u+=2 )
		{
			if( memcmp( (void*)(u), FSAccessPattern, sizeof(FSAccessPattern) ) == 0 )
			{
				//gprintf("FSAccessPatch:%08X\r\n", u );
				memcpy( (void*)u, FSAccessPatch, sizeof(FSAccessPatch) );
				DCFlushRange((void*)u, sizeof(FSAccessPatch));
				break;
			}
		}

		// Load and patch IOS58.
		if (LoadKernel() < 0)
		{
			// NOTE: Attempting to initialize controllers here causes a crash.
			// Hence, we can't wait for the user to press the HOME button, so
			// we'll just wait for a timeout instead.
			PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*20, "Returning to loader in 10 seconds.");
			UpdateScreen();
			VIDEO_WaitVSync();

			// Wait 10 seconds...
			usleep(10000000);

			// Return to the loader.
			ExitToLoader(1);
		}
		PatchKernel();
		u32 v = FoundVersion;
		//this ensures all IOS modules get loaded in ES on reload
		memcpy( ESBootPatch+0x14, &v, 4 );
		DCInvalidateRange( (void*)0x939F0348, sizeof(ESBootPatch) );
		memcpy( (void*)0x939F0348, ESBootPatch, sizeof(ESBootPatch) );
		DCFlushRange( (void*)0x939F0348, sizeof(ESBootPatch) );

		// Loading IOS58 Kernel...
		if(argsboot == false)
			ShowMessageScreen("Loading IOS58 Kernel...");

		//libogc still has that, lets close it
		__ES_Close();
		fd = IOS_Open( dev_es, 0 );

		raw_irq_handler_t irq_handler = BeforeIOSReload();
		IOS_IoctlvAsync( fd, 0x25, 0, 0, IOCTL_Buf, NULL, NULL );
		sleep(1); //wait this time at least
		AfterIOSReload( irq_handler, v );
		//Disables MEMPROT for patches
		write16(MEM_PROT, 0);
	}
	//else if(*(vu16*)0xCD8005A0 != 0xCAFE)
	//{
		/* WiiVC seems to have some bug that without any fake IOS
		   reload makes it impossible to read HW regs on PPC 
		   and it seems to break some consoles to reload IOS */
	//	IOS_ReloadIOS(58);
	//}

	// Preparing Nintendont Kernel...
	if(argsboot == false)
		ShowMessageScreen("Preparing Nintendont Kernel...");

	//inject nintendont thread
	void *kernel_bin = NULL;
	unsigned int kernel_bin_size = 0;
	unzip_data(kernel_zip, kernel_zip_size, &kernel_bin, &kernel_bin_size);
	gprintf("Decompressed kernel.bin with %i bytes\r\n", kernel_bin_size);
	memcpy((void*)0x92F00000,kernel_bin,kernel_bin_size);
	DCFlushRange((void*)0x92F00000,kernel_bin_size);
	free(kernel_bin);
	//inject kernelboot
	memcpy((void*)0x92FFFE00,kernelboot_bin,kernelboot_bin_size);
	DCFlushRange((void*)0x92FFFE00,kernelboot_bin_size);
	//Loading Nintendont Kernel...
	if(argsboot == false)
		ShowMessageScreen("Loading Nintendont Kernel...");
	//close in case this is wii vc
	__ES_Close();
	memset( STATUS, 0, 0x20 );
	DCFlushRange( STATUS, 0x20 );
	//make sure kernel doesnt reload
	*(vu32*)0x93003420 = 0;
	DCFlushRange((void*)0x93003420,0x20);
	//Set some important kernel regs
	*(vu32*)0x92FFFFC0 = isWiiVC; //cant be detected in IOS
	if(WiiDRC_Connected()) //used in PADReadGC.c
		*(vu32*)0x92FFFFC4 = (u32)WiiDRC_GetRawI2CAddr();
	else //will disable gamepad spot for player 1
		*(vu32*)0x92FFFFC4 = 0;
	DCFlushRange((void*)0x92FFFFC0,0x20);
	fd = IOS_Open( dev_es, 0 );
	IOS_IoctlvAsync(fd, 0x1F, 0, 0, IOCTL_Buf, NULL, NULL);
	//Waiting for Nintendont...
	if(argsboot == false)
		ShowMessageScreen("Waiting for Nintendont...");
	while(1)
	{
		DCInvalidateRange( STATUS, 0x20 );
		if((STATUS_LOADING > 0 || abs(STATUS_LOADING) > 1) && STATUS_LOADING < 20)
		{
			gprintf("Kernel sent signal\n");
			break;
		}
	}
	//Async Ioctlv done by now
	IOS_Close(fd);

	gprintf("Nintendont at your service!\r\n%s\r\n", NIN_BUILD_STRING);
	KernelLoaded = 1;

	// Checking for storage devices...
	if(argsboot == false)
		ShowMessageScreen("Checking storage devices...");

	// Initialize devices.
	// TODO: Only mount the device Nintendont was launched from
	// Mount the other device asynchronously.
	bool foundOneDevice = false;
	int i;
	for (i = DEV_SD; i <= DEV_USB; i++)
	{
		//only check SD on Wii VC
		if(i == DEV_USB && isWiiVC)
			break;
		//check SD and USB on Wii and WiiU
		const WCHAR *devNameFF = MountDevice(i);
		if (devNameFF && !foundOneDevice)
		{
			// Set this device as primary.
			primaryDevice = devNameFF;
			changeToDefaultDrive();
			foundOneDevice = true;
		}
	}

	// FIXME: Show this information in the menu instead of
	// aborting here.
	if (!devices[DEV_SD] && !devices[DEV_USB])
	{
		ClearScreen();
		gprintf("No FAT device found!\n");
		PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, 232, "No FAT device found!");
		ExitToLoader(1);
	}
	// Seems like some programs start without any args
	if(argc > 0 && argv != NULL && argv[0] != NULL)
	{
		char* first_slash = strrchr(argv[0], '/');
		if (first_slash != NULL) strncpy(launch_dir, argv[0], first_slash-argv[0]+1);
	}
	gprintf("launch_dir = %s\r\n", launch_dir);

	// Initialize controllers.
	// FIXME: Initialize before storage devices.
	// Doing that right now causes usbstorage to fail...
	FPAD_Init();
	FPAD_Update();

	/* Read IPL Font before doing any patches */
	void *fontbuffer = memalign(32, 0x50000);
	__SYS_ReadROM((void*)fontbuffer,0x50000,0x1AFF00);
	memcpy((void*)0xD3100000, fontbuffer, 0x50000);
	DCInvalidateRange( (void*)0x93100000, 0x50000 );
	free(fontbuffer);
	//gprintf("Font: 0x1AFF00 starts with %.4s, 0x1FCF00 with %.4s\n", (char*)0x93100000, (char*)0x93100000 + 0x4D000);

	// Update meta.xml.
	updateMetaXml();

	if(argsboot == false)
	{
		// Load titles.txt.
		LoadTitles();

		if (LoadNinCFG() == false)
		{
			memset(ncfg, 0, sizeof(NIN_CFG));

			ncfg->Magicbytes = 0x01070CF6;
			ncfg->Version = NIN_CFG_VERSION;
			ncfg->Language = NIN_LAN_AUTO;
			ncfg->MaxPads = NIN_CFG_MAXPAD;
			ncfg->MemCardBlocks = 0x2;//251 blocks
		}

		// Prevent autobooting if B is pressed
		int i = 0;
		while((ncfg->Config & NIN_CFG_AUTO_BOOT) && i < 1000000) // wait for wiimote re-synch
		{
			if (i == 0) {
				PrintInfo();
				PrintFormat(DEFAULT_SIZE, BLACK, 320 - 90, MENU_POS_Y + 20*10, "B: Cancel Autoboot");
				GRRLIB_Render();
				ClearScreen();
			}
			
			FPAD_Update();

			if (FPAD_Cancel(0)) {
				ncfg->Config &= ~NIN_CFG_AUTO_BOOT;
				break;
			}

			i++;
		}
	}
	ReconfigVideo(rmode);
	UseSD = (ncfg->Config & NIN_CFG_USB) == 0;

	bool progressive = (CONF_GetProgressiveScan() > 0) && VIDEO_HaveComponentCable();
	if(progressive) //important to prevent blackscreens
		ncfg->VideoMode |= NIN_VID_PROG;
	else
		ncfg->VideoMode &= ~NIN_VID_PROG;

	bool SaveSettings = false;
	if(!(ncfg->Config & NIN_CFG_AUTO_BOOT))
	{
		// Not autobooting.
		// Prompt the user to select a device and game.
		SaveSettings = SelectDevAndGame();
	}
	else
	{
		// Autobooting.
		gprintf("Autobooting:\"%s\"\r\n", ncfg->GamePath );
		//this aparently can break some vc autoboot issues
                //PrintInfo();
		//GRRLIB_Render();
		//ClearScreen();
	}

//Init DI and set correct ID if needed
	unsigned int CurDICMD = 0;
	if( memcmp(ncfg->GamePath, "di", 3) == 0 )
	{
		if(argsboot == false)
			ShowLoadingScreen();
		if(isWiiVC)
		{
			if(WDVD_Init() != 0)
				ShowMessageScreenAndExit("The Wii VC Disc could not be initialized!", 1);
			if(WDVD_OpenDataPartition() != 0)
				ShowMessageScreenAndExit("Found no Partition on Wii VC Disc!", 1);
			if(!WDVD_FST_Mount())
				ShowMessageScreenAndExit("Unable to open Partition on Wii VC Disc!", 1);
			if(WDVD_FST_OpenDisc(0) != 0)
				ShowMessageScreenAndExit("No game.iso on Wii VC Disc!", 1);
			u8 *DIBuf = memalign(32,0x800);
			if(WDVD_FST_Read(DIBuf, 0x800) != 0x800)
			{
				free(DIBuf);
				ShowMessageScreenAndExit("Cant read game.iso start!", 1);
			}
			if( IsGCGame(DIBuf) == false )
			{
				WDVD_FST_LSeek(0x8000);
				WDVD_FST_Read(DIBuf, 0x800);
				if( IsGCGame(DIBuf) == false )
				{
					free(DIBuf);
					ShowMessageScreenAndExit("game.iso is not a GC Disc!", 1);
				}
			}
			memcpy(&(ncfg->GameID), DIBuf, 4);
			free(DIBuf);
			WDVD_FST_Close();
			wiiVCInternal = true;
		}
		else
		{
			DI_UseCache(false);
			DI_Init();
			DI_Mount();
			while (DI_GetStatus() & DVD_INIT)
				usleep(20000);
			if(!(DI_GetStatus() & DVD_READY))
			{
				ShowMessageScreenAndExit("The Disc Drive could not be initialized!", 1);
			}
			DI_Close();

			u8 *DIBuf = memalign(32,0x800);
			memset(DIBuf, 0, 0x20);
			DCFlushRange(DIBuf, 0x20);
			CurDICMD = DIP_CMD_NORMAL;
			ReadRealDisc(DIBuf, 0, 0x20, CurDICMD);
			if( IsGCGame(DIBuf) == false )
			{
				memset(DIBuf, 0, 0x800);
				DCFlushRange(DIBuf, 0x800);
				CurDICMD = DIP_CMD_DVDR;
				ReadRealDisc(DIBuf, 0, 0x800, CurDICMD);
				if( IsGCGame(DIBuf) == false )
				{
					free(DIBuf);
					ShowMessageScreenAndExit("The Disc in the Drive is not a GC Disc!", 1);
				}
			}
			memcpy(&(ncfg->GameID), DIBuf, 4);
			free(DIBuf);
		}
	}

	if(SaveSettings)
	{
		// TODO: If the boot device is the same as the game device,
		// don't write it twice.

		// Write config to the boot device, which is loaded on next launch.
		FIL cfg;
		if (f_open_char(&cfg, "/nincfg.bin", FA_WRITE|FA_OPEN_ALWAYS) == FR_OK)
		{
			// Reserve space in the file.
			if (f_size(&cfg) < sizeof(NIN_CFG)) {
				f_expand(&cfg, sizeof(NIN_CFG), 1);
			}

			// Write nincfg.bin.
			UINT wrote;
			f_write(&cfg, ncfg, sizeof(NIN_CFG), &wrote);
			f_close(&cfg);
		}

		// Write config to the game device, used by the Nintendont kernel.
		char ConfigPath[20];
		snprintf(ConfigPath, sizeof(ConfigPath), "%s:/nincfg.bin", GetRootDevice());
		if (f_open_char(&cfg, ConfigPath, FA_WRITE|FA_OPEN_ALWAYS) == FR_OK)
		{
			// Reserve space in the file.
			if (f_size(&cfg) < sizeof(NIN_CFG)) {
				f_expand(&cfg, sizeof(NIN_CFG), 1);
			}

			// Write nincfg.bin.
			UINT wrote;
			f_write(&cfg, ncfg, sizeof(NIN_CFG), &wrote);
			f_close(&cfg);
		}

		FlushDevices();
	}

	// Get multi-game and region code information.
	u32 ISOShift = 0;	// NOTE: This is a 34-bit shifted offset.
	u32 BI2region = 0;	// bi2.bin region code [TODO: Validate?]
	int ret = CheckForMultiGameAndRegion(CurDICMD, &ISOShift, &BI2region);
	if (ret != 0) {
		ClearScreen();
		PrintInfo();
		PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*4, "CheckForMultiGameAndRegion() failed: %d", ret);
		switch (ret) {
			case 1:
				PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*5, "Unable to open the %s.",
					CurDICMD == 0 ? "disc image file" : "disc drive");
				break;
			case 2:
				PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*5, "Unable to read the disc header.");
				break;
			case 3:
				PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*5, "Unable to read the CISO bi2.bin area.");
				break;
			case 4:
				PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*5, "Unable to open the extracted FST bi2.bin file.");
				break;
			case 5:
				PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*5, "Unable to read the extracted FST bi2.bin file.");
				break;
			default:
				PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*5, "Unknown error code.");
				break;
		}

		if (CurDICMD) {
			char unkdev[32];
			const char *device;
			switch (CurDICMD) {
				case DIP_CMD_NORMAL:
					device = "GameCube disc";
					break;
				case DIP_CMD_DVDR:
					device = "DVD-R";
					break;
				default:
					snprintf(unkdev, sizeof(unkdev), "Unknown (CMD: 0x%02x)", CurDICMD);
					device = unkdev;
					break;
			}
			PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*7, "Device: %s", device);
		} else {
			PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*7, "Filename: %s:%s", GetRootDevice(), ncfg->GamePath);
		}

		PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*20, "Returning to loader in 10 seconds.");
		UpdateScreen();
		VIDEO_WaitVSync();

		// Wait 10 seconds...
		usleep(10000000);

		// Return to the loader.
		ExitToLoader(1);
	}

	// Save the ISO shift value for multi-game discs.
	*(vu32*)0xD300300C = ISOShift;

//Set Language
	if(ncfg->Language == NIN_LAN_AUTO || ncfg->Language >= NIN_LAN_LAST)
	{
		if(BI2region == BI2_REGION_PAL)
		{
			switch (CONF_GetLanguage())
			{
				case CONF_LANG_GERMAN:
					ncfg->Language = NIN_LAN_GERMAN;
					break;
				case CONF_LANG_FRENCH:
					ncfg->Language = NIN_LAN_FRENCH;
					break;
				case CONF_LANG_SPANISH:
					ncfg->Language = NIN_LAN_SPANISH;
					break;
				case CONF_LANG_ITALIAN:
					ncfg->Language = NIN_LAN_ITALIAN;
					break;
				case CONF_LANG_DUTCH:
					ncfg->Language = NIN_LAN_DUTCH;
					break;
				default:
					ncfg->Language = NIN_LAN_ENGLISH;
					break;
			}
		}
		else
			ncfg->Language = NIN_LAN_ENGLISH;
	}

	if(ncfg->Config & NIN_CFG_MEMCARDEMU)
	{
		// Memory card emulation is enabled.
		// Set up the memory card file.
		char BasePath[20];
		snprintf(BasePath, sizeof(BasePath), "%s:/saves", GetRootDevice());
		f_mkdir_char(BasePath);

		char MemCardName[8];
		memset(MemCardName, 0, 8);
		if ( ncfg->Config & NIN_CFG_MC_MULTI )
		{
			// "Multi" mode is enabled.
			// Use one memory card for USA/PAL games,
			// and another memory card for JPN games.
			switch (BI2region)
			{
				case BI2_REGION_JAPAN:
				case BI2_REGION_SOUTH_KOREA:
				default:
					// JPN games.
					memcpy(MemCardName, "ninmemj", 7);
					break;

				case BI2_REGION_USA:
				case BI2_REGION_PAL:
					// USA/PAL games.
					memcpy(MemCardName, "ninmem", 6);
					break;
			}
		}
		else
		{
			// One card per game.
			memcpy(MemCardName, &(ncfg->GameID), 4);
		}

		char MemCard[32];
		snprintf(MemCard, sizeof(MemCard), "%s/%s.raw", BasePath, MemCardName);
		gprintf("Using %s as Memory Card.\r\n", MemCard);
		FIL f;
		if (f_open_char(&f, MemCard, FA_READ|FA_OPEN_EXISTING) != FR_OK)
		{
			// Memory card file not found. Create it.
			if(GenerateMemCard(MemCard, BI2region) == false)
			{
				ClearScreen();
				ShowMessageScreenAndExit("Failed to create Memory Card File!", 1);
			}
		}
		else
		{
			// Memory card file found.
			f_close(&f);
		}
	}
	else
	{
		// Using real memory card slots. (Wii only)
		// Setup real SRAM language.
		syssram *sram;
		sram = __SYS_LockSram();
		sram->lang = ncfg->Language;
		__SYS_UnlockSram(1); // 1 -> write changes
		while(!__SYS_SyncSram());

		// Send game ID to MemCard Pro GC
		s32 chan;
		for ( chan = 0; chan < 2; chan++) {
			if (CheckForMemCardPro(chan)) {
				SetMemCardProGameInfo(chan, ncfg);
			}
		}
	}
	
	//Check if game is Triforce game
	u32 IsTRIGame = 0;
	if (ncfg->GameID != 0x47545050) //Damn you Knights Of The Temple!
		IsTRIGame = TRISetupGames(ncfg->GamePath, CurDICMD, ISOShift);
	
	if(IsTRIGame != 0)
	{
		// Create saves directory.
		char BasePath[20];
		snprintf(BasePath, sizeof(BasePath), "%s:/saves", GetRootDevice());
		f_mkdir_char(BasePath);
	}

	#define GCN_IPL_SIZE 2097152
	#define TRI_IPL_SIZE 1048576
	void *iplbuf = NULL;
	bool useipl = false;
	bool useipltri = false;

	if (!(ncfg->Config & (NIN_CFG_SKIP_IPL)))
	{
		if(IsTRIGame == 0)
		{
			// Attempt to load the GameCube IPL.
			char iplchar[32];
			iplchar[0] = 0;
			switch (BI2region)
			{
				case BI2_REGION_USA:
					snprintf(iplchar, sizeof(iplchar), "%s:/iplusa.bin", GetRootDevice());
					break;

				case BI2_REGION_JAPAN:
				case BI2_REGION_SOUTH_KOREA:
				default:
					snprintf(iplchar, sizeof(iplchar), "%s:/ipljap.bin", GetRootDevice());
					break;

				case BI2_REGION_PAL:
					// FIXME: PAL IPL is broken on Wii U.
					if (!IsWiiU())
						snprintf(iplchar, sizeof(iplchar), "%s:/iplpal.bin", GetRootDevice());
					break;
			}

			FIL f;
			if (iplchar[0] != 0 &&
			    f_open_char(&f, iplchar, FA_READ|FA_OPEN_EXISTING) == FR_OK)
			{
				if (f.obj.objsize == GCN_IPL_SIZE)
				{
					iplbuf = malloc(GCN_IPL_SIZE);
					UINT read;
					f_read(&f, iplbuf, GCN_IPL_SIZE, &read);
					useipl = (read == GCN_IPL_SIZE);
				}
				f_close(&f);
			}
		}
		else
		{
			// Attempt to load the Triforce IPL. (segaboot)
			char iplchar[32];
			snprintf(iplchar, sizeof(iplchar), "%s:/segaboot.bin", GetRootDevice());
			FIL f;
			if (f_open_char(&f, iplchar, FA_READ|FA_OPEN_EXISTING) == FR_OK)
			{
				if (f.obj.objsize == TRI_IPL_SIZE)
				{
					f_lseek(&f, 0x20);
					void *iplbuf = (void*)0x92A80000;
					UINT read;
					f_read(&f, iplbuf, TRI_IPL_SIZE - 0x20, &read);
					useipltri = (read == (TRI_IPL_SIZE - 0x20));
				}
				f_close(&f);
			}
		}
	}

	//sync changes
	CloseDevices();

	WPAD_Disconnect(0);
	WPAD_Disconnect(1);
	WPAD_Disconnect(2);
	WPAD_Disconnect(3);

	WUPC_Shutdown();
	WPAD_Shutdown();

	if(wiiVCInternal)
	{
		WDVD_FST_Unmount();
		WDVD_Close();
	}

	//before flushing do game specific patches
	if(ncfg->Config & NIN_CFG_FORCE_PROG &&
			ncfg->GameID == 0x47584745)
	{	//Mega Man X Collection does progressive ingame so
		//forcing it would mess with the interal game setup
		gprintf("Disabling Force Progressive for this game\r\n");
		ncfg->Config &= ~NIN_CFG_FORCE_PROG;
	}

	//make sure the cfg gets to the kernel
	DCStoreRange((void*)ncfg, sizeof(NIN_CFG));

//set current time
	u32 bias = 0, cur_time = 0;
	__SYS_GetRTC(&cur_time);
	if(CONF_GetCounterBias(&bias) >= 0)
		cur_time += bias;
	settime(secs_to_ticks(cur_time));
//hand over approx time passed since 1970
	*(vu32*)0xD3003424 = (cur_time+946684800);

//set status for kernel to start running
	*(vu32*)0xD3003420 = 0x0DEA;
	while(1)
	{
		DCInvalidateRange( STATUS, 0x20 );
		if( STATUS_LOADING == 0xdeadbeef )
			break;

		if(argsboot == false)
		{
			PrintInfo();

			PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*6, "Loading patched kernel... %d", STATUS_LOADING);
			if(STATUS_LOADING == 0)
			{
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*7, "ES_Init...");
				// Cleans the -1 when it's past it to avoid confusion if another error happens. e.g. before it showed "81" instead of "8" if the controller was unplugged.
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 163, MENU_POS_Y + 20*6, " ");
			}
			if((STATUS_LOADING > 0 || abs(STATUS_LOADING) > 1) && STATUS_LOADING < 20)
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*7, "ES_Init... Done!");
			if(STATUS_LOADING == 2)
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*8, "Initing storage devices...");
			if(abs(STATUS_LOADING) > 2 && abs(STATUS_LOADING) < 20)
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*8, "Initing storage devices... Done!");
			if(STATUS_LOADING == -2)
				PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*8, "Initing storage devices... Error! %d  Shutting down", STATUS_ERROR);
			if(STATUS_LOADING == 3)
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*9, "Mounting USB/SD device...");
			if(abs(STATUS_LOADING) > 3 && abs(STATUS_LOADING) < 20)
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*9, "Mounting USB/SD device... Done!");
			if(STATUS_LOADING == -3)
				PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*9, "Mounting USB/SD device... Error! %d  Shutting down", STATUS_ERROR);
			if(STATUS_LOADING == 5) {
	/* 			if (timeout == 0)
					timeout = ticks_to_secs(gettime()) + 20; // Set timer for 20 seconds
				else if (timeout <= ticks_to_secs(gettime())) {
					STATUS_ERROR = -7;
					DCFlushRange(STATUS, 0x20);
					usleep(100);
					//memset( (void*)0x92f00000, 0, 0x100000 );
					//DCFlushRange( (void*)0x92f00000, 0x100000 );
					//ExitToLoader(1);
				}*/
				PrintFormat(DEFAULT_SIZE, (STATUS_ERROR == -7) ? MAROON:BLACK, MENU_POS_X, MENU_POS_Y + 20*10, (STATUS_ERROR == -7) ? "Checking FS... Timeout!" : "Checking FS...");
			}
			if(abs(STATUS_LOADING) > 5 && abs(STATUS_LOADING) < 20)
			{
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*10, "Checking FS... Done!");
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*11, "Drive size: %.02f%s Sector size: %d", STATUS_DRIVE, STATUS_GB_MB ? "GB" : "MB", STATUS_SECTOR);
			}
			if(STATUS_LOADING == -5)
				PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*10, "Checking FS... Error! %d Shutting down", STATUS_ERROR);
			if(STATUS_LOADING == 6)
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*12, "ES_LoadModules...");
			if(abs(STATUS_LOADING) > 6 && abs(STATUS_LOADING) < 20)
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*12, "ES_LoadModules... Done!");
			if(STATUS_LOADING == -6)
				PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*12, "ES_LoadModules... Error! %d Shutting down", STATUS_ERROR);
			if(STATUS_LOADING == 7)
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*13, "Loading config...");
			if(abs(STATUS_LOADING) > 7 && abs(STATUS_LOADING) < 20)
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*13, "Loading config... Done!");
			/*if(STATUS_LOADING == 8)
			{
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*14, "Init HID devices... ");
				if ( STATUS_ERROR == 1)
				{
					PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*15, "          Make sure the Controller is plugged in");
				}
				else
					PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*15, "%50s", " ");
			}
			if(abs(STATUS_LOADING) > 8 && abs(STATUS_LOADING) < 20)
			{
				if (ncfg->Config & NIN_CFG_NATIVE_SI)
					PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*14, "Init HID devices... Using ONLY NATIVE Gamecube Ports");
				else if ((ncfg->MaxPads == 1) && (ncfg->Config & NIN_CFG_HID))
					PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*14, "Init HID devices... Using Gamecube and HID Ports");
				else if ((ncfg->MaxPads > 0) && (ncfg->Config & NIN_CFG_HID))
					PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*14, "Init HID devices... Using Gamecube, HID, and BT Ports");
				else if (ncfg->MaxPads > 0)
					PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*14, "Init HID devices... Using Gamecube and BT Ports");
				else if (ncfg->Config & NIN_CFG_HID)
					PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*14, "Init HID devices... Using HID and Bluetooth Ports");
				else
					PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*14, "Init HID devices... Using Bluetooth Ports... Done!");
			}
			if(STATUS_LOADING == -8)
			{
				PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*14, "Init HID devices... Failed! Shutting down");
				switch (STATUS_ERROR)
				{
					case -1:
						PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*15, "No Controller plugged in! %25s", " ");
						break;
					case -2:
						PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*15, "Missing %s:/controller.ini %20s", GetRootDevice(), " ");
						break;
					case -3:
						PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*15, "Controller does not match %s:/controller.ini %6s", GetRootDevice(), " ");
						break;
					case -4:
						PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*15, "Invalid Polltype in %s:/controller.ini %12s", GetRootDevice(), " ");
						break;
					case -5:
						PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*15, "Invalid DPAD value in %s:/controller.ini %9s", GetRootDevice(), " ");
						break;
					case -6:
						PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*15, "PS3 controller init error %25s", " ");
						break;
					case -7:
						PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*15, "Gamecube adapter for Wii u init error %13s", " ");
						break;
					default:
						PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*15, "Unknown error %d %35s", STATUS_ERROR, " ");
						break;
				}
			}*/
			if(STATUS_LOADING == 9)
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*14, "Init DI... %40s", " ");
			if(abs(STATUS_LOADING) > 9 && abs(STATUS_LOADING) < 20)
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*14, "Init DI... Done! %35s", " ");
			if(STATUS_LOADING == 10)
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*15, "Init CARD...");
			if(abs(STATUS_LOADING) > 10 && abs(STATUS_LOADING) < 20)
				PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*15, "Init CARD... Done!");
			GRRLIB_Screen2Texture(0, 0, screen_buffer, GX_FALSE); // Copy all status messages
			GRRLIB_Render();
			ClearScreen();
		}
		while((STATUS_LOADING < -1) && (STATUS_LOADING > -20)) //displaying a fatal error
				; //do nothing wait for shutdown
	}
	if(argsboot == false)
	{
		DrawBuffer(); // Draw all status messages
		PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*17, "Nintendont kernel looping, loading game...");
		GRRLIB_Render();
		DrawBuffer(); // Draw all status messages
	}
//	memcpy( (void*)0x80000000, (void*)0x90140000, 0x1200000 );
	GRRLIB_FreeTexture(background);
	GRRLIB_FreeTexture(screen_buffer);
	GRRLIB_FreeTTF(myFont);
	GRRLIB_Exit();

	gprintf("GameRegion:");

	u32 vidForce = (ncfg->VideoMode & NIN_VID_FORCE);
	u32 vidForceMode = (ncfg->VideoMode & NIN_VID_FORCE_MASK);

	progressive = (ncfg->Config & NIN_CFG_FORCE_PROG)
		&& !useipl && !useipltri;

	switch (BI2region)
	{
		case BI2_REGION_PAL:
			if (progressive || (vidForce &&
			    (vidForceMode == NIN_VID_FORCE_PAL60 ||
			     vidForceMode == NIN_VID_FORCE_MPAL ||
			     vidForceMode == NIN_VID_FORCE_NTSC)))
			{
				// PAL60 and/or PAL-M
				*(vu32*)0x800000CC = 5;
			}
			else
			{
				// PAL50
				*(vu32*)0x800000CC = 1;
			}
			vmode = &TVPal528IntDf;
			break;

		case BI2_REGION_USA:
			if ((vidForce && vidForceMode == NIN_VID_FORCE_MPAL) ||
			    (!vidForce && ((CONF_GetVideo() == CONF_VIDEO_MPAL) 
					|| (useipl && memcmp(iplbuf+0x55,"MPAL",4) == 0))))
			{
				// PAL-M
				*(vu32*)0x800000CC = 3;
				vmode = &TVMpal480IntDf;
			}
			else
			{
				// NTSC
				*(vu32*)0x800000CC = 0;
				vmode = &TVNtsc480IntDf;
			}
			break;

		case BI2_REGION_JAPAN:
		case BI2_REGION_SOUTH_KOREA:
		default:
			// NTSC
			*(vu32*)0x800000CC = 0;
			vmode = &TVNtsc480IntDf;
			break;
	}

	if(progressive)
		vmode = &TVNtsc480Prog;
	else if(vidForce)
	{
		switch(vidForceMode)
		{
			case NIN_VID_FORCE_PAL50:
				vmode = &TVPal528IntDf;
				break;
			case NIN_VID_FORCE_PAL60:
				vmode = &TVEurgb60Hz480IntDf;
				break;
			case NIN_VID_FORCE_MPAL:
				vmode = &TVMpal480IntDf;
				break;
			case NIN_VID_FORCE_NTSC:
			default:
				vmode = &TVNtsc480IntDf;
				break;
		}
	}
	if((ncfg->Config & NIN_CFG_MEMCARDEMU) == 0) //setup real sram video
	{
		syssram *sram;
		sram = __SYS_LockSram();
		sram->display_offsetH = 0;	// Clear Offset
		sram->flags		&= ~0x80;	// Clear Progmode
		sram->flags		&= ~3;		// Clear Videomode

		// PAL60 flag.
		if (BI2region == BI2_REGION_PAL)
		{
			// Enable PAL60.
			sram->ntd |= 0x40;

			// TODO: Set the progressive scan flag on PAL?
		}
		else
		{
			// Disable PAL60.
			sram->ntd &= 0x40;

			// NTSC Prince of Persia Warrior Within set to Spanish
			// actually has a bug that cant display the progressive
			// screen question so dont set progressive flag in that
			bool spPopWW = (ncfg->GameID == 0x47324F45 &&
							ncfg->Language == NIN_LAN_SPANISH);

			// Set the progressive scan flag if a component cable
			// is connected (or HDMI on Wii U), unless we're loading
			// BMX XXX, since that game won't even boot on a real
			// GameCube if a component cable is connected.
			if ((ncfg->GameID >> 8) != 0x474233 && !spPopWW &&
				(ncfg->VideoMode & NIN_VID_PROG))
			{
				sram->flags |= 0x80;
			}
		}

		if(*(vu32*)0x800000CC == 1 || *(vu32*)0x800000CC == 5)
			sram->flags	|= 1; //PAL Video Mode
		__SYS_UnlockSram(1); // 1 -> write changes
		while(!__SYS_SyncSram());
	}
	
	ReconfigVideo(vmode);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(vmode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync();
	else while(VIDEO_GetNextField())
		VIDEO_WaitVSync();

	*(u16*)(0xCC00501A) = 156;	// DSP refresh rate
	/* from libogc, get all gc pads to work */
	u32 buf[2];
	u32 bits = 0;
	u8 chan;
	for(chan = 0; chan < 4; ++chan)
	{
		bits |= (0x80000000>>chan);
		SI_GetResponse(chan,(void*)buf);
		SI_SetCommand(chan,(0x00000300|0x00400000));
		SI_EnablePolling(bits);
	}
	*(vu32*)0xD3003004 = 1; //ready up HID Thread

	VIDEO_SetBlack(TRUE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(vmode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync();
	else while(VIDEO_GetNextField())
		VIDEO_WaitVSync();
	GX_AbortFrame();

	DCFlushRange((void*)0x93000000, 0x3000);

	DCInvalidateRange((void*)0x93006000, 0xA000);
	memset((void*)0x93006000, 0, 0xA000);
	memcpy((void*)0x93006000, IOSInterface_bin, IOSInterface_bin_size);
	DCFlushRange((void*)0x93006000, 0xA000);

	DCInvalidateRange((void*)0x93010010, 0x10000);
	memcpy((void*)0x93010010, loader_stub, 0x1800);
	memcpy((void*)0x93011810, stub_bin, stub_bin_size);
	DCFlushRange((void*)0x93010010, 0x10000);

	DCInvalidateRange((void*)0x93020000, 0x10000);
	memset((void*)0x93020000, 0, 0x10000);
	DCFlushRange((void*)0x93020000, 0x10000);

	DCInvalidateRange((void*)0x93003000, 0x200);
	//*(vu32*)0x93003000 = currev; //set kernel rev (now in LoadKernel)
	*(vu32*)0x93003008 = 0x80000004; //just some address for SIGetType
	//0x9300300C is already used for multi-iso
	memset((void*)0x93003010, 0, 0x190); //clears alot of pad stuff
	strcpy((char*)0x930031A0, "ARStartDMA: %08x %08x %08x\n"); //ARStartDMA Debug
	memset((void*)0x930031E0, 0, 0x20); //clears tgc stuff
	DCFlushRange((void*)0x93003000, 0x200);

	//lets prevent weird events
	__STM_Close();

	//THIS thing right here, it interrupts some games and breaks them
	//To fix that, call ExecSuspendScheduler so WC24 just sleeps
	u32 out = 0;
	fd = IOS_Open("/dev/net/kd/request", 0);
	IOS_Ioctl(fd, IOCTL_ExecSuspendScheduler, NULL, 0, &out, 4);
	IOS_Close(fd);

	if(ncfg->Config & NIN_CFG_BBA_EMU)
	{
		ncfg->NetworkProfile &= 3;
		if(ncfg->NetworkProfile > 0)
		{
			do {
				fd = IOS_Open("/dev/net/ncd/manage", 0);
			} while(fd < 0);
			//loader_stub is unused at this point so...
			IOCTL_Buf[0].data = loader_stub;
			IOCTL_Buf[0].len = 0x1B5C;
			IOCTL_Buf[1].data = loader_stub+0x1B60;
			IOCTL_Buf[1].len = 0x20;
			IOS_Ioctlv(fd, 3, 0, 2, IOCTL_Buf);
			//enable requested profile
			if(ncfg->NetworkProfile == 1)
			{
				//enable profile 1
				loader_stub[0x8] |= 0xA0;
				//set wireless/wired select
				if(loader_stub[0x8]&1) //wired
					loader_stub[0x4] = 2;
				else //wireless
					loader_stub[0x4] = 1;
				//disable profile 2 and 3
				loader_stub[0x924] &= 0x7F;
				loader_stub[0x1240] &= 0x7F;
			}
			else if(ncfg->NetworkProfile == 2)
			{
				//disable profile 1
				loader_stub[0x8] &= 0x7F;
				//enable profile 2
				loader_stub[0x924] |= 0xA0;
				//set wireless/wired select
				if(loader_stub[0x924]&1) //wired
					loader_stub[0x4] = 2;
				else //wireless
					loader_stub[0x4] = 1;
				//disable profile 3
				loader_stub[0x1240] &= 0x7F;
			}
			else //if(ncfg->NetworkProfile == 3)
			{
				//disable profile 1 and 2
				loader_stub[0x8] &= 0x7F;
				loader_stub[0x924] &= 0x7F;
				//enable profile 3
				loader_stub[0x1240] |= 0xA0;
				//set wireless/wired select
				if(loader_stub[0x1240]&1) //wired
					loader_stub[0x4] = 2;
				else //wireless
					loader_stub[0x4] = 1;
			}
			//flush to RAM, REALLY important
			DCFlushRange(loader_stub, 0x1C00);
			IOS_Ioctlv(fd, 4, 1, 1, IOCTL_Buf);
			IOS_Close(fd);
		}
	}

	write16(0xD8B420A, 0); //disable MEMPROT again after reload
	//u32 level = IRQ_Disable();
	__exception_closeall();
	__lwp_thread_closeall();

	DVDStartCache(); //waits for kernel start
	DCInvalidateRange((void*)0x90000000, 0x1000000);
	memset((void*)(void*)0x90000000, 0, 0x1000000); //clear ARAM
	DCFlushRange((void*)0x90000000, 0x1000000);

	gprintf("Game Start\n");
	//alow interrupts on Y2
	write32(0x0d000004,0x22);
	if(useipl)
	{
		load_ipl(iplbuf);
		*(vu32*)0xD3003420 = 0x5DEA;
		while(*(vu32*)0xD3003420 == 0x5DEA) ;
		/* Patches */
		DCInvalidateRange((void*)0x80001000, 0x2000);
		ICInvalidateRange((void*)0x80001000, 0x2000);
		/* IPL */
		DCInvalidateRange((void*)0x81300000, 0x300000);
		ICInvalidateRange((void*)0x81300000, 0x300000);
		/* Seems to boot more stable this way */
		//gprintf("Using 32kHz DSP (No Resample)\n");
		write32(0xCD806C00, 0x68);
		free(iplbuf);
	}
	else //use our own loader
	{
		if(useipltri)
		{
			*(vu32*)0xD3003420 = 0x6DEA;
			while(*(vu32*)0xD3003420 == 0x6DEA) ;
		}
		memcpy((void*)0x81300000, multidol_ldr_bin, multidol_ldr_bin_size);
		DCFlushRange((void*)0x81300000, multidol_ldr_bin_size);
		ICInvalidateRange((void*)0x81300000, multidol_ldr_bin_size);
	}
	_jmp813();

	//IRQ_Restore(level);

	return 0;
}
