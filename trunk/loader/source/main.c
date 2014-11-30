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

#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <wupc/wupc.h>
#include <fat.h>
#include <sys/dir.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <network.h>
#include <sys/errno.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/lwp_threads.h>
#include <ogc/isfs.h>
#include <ogc/ipc.h>

#include <dirent.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <fcntl.h>

#include "exi.h"
#include "dip.h"
#include "global.h"
#include "font.h"
#include "Config.h"
#include "FPad.h"
#include "menu.h"
#include "Patches.h"
#include "kernel_bin.h"
#include "PADReadGC_bin.h"
#include "multidol_ldr_bin.h"
#include "stub_bin.h"
#include "titles.h"

extern void __exception_setreload(int t);
extern void __SYS_ReadROM(void *buf,u32 len,u32 offset);
extern u32 __SYS_GetRTC(u32 *gctime);

#define STATUS			((void*)0x90004100)
#define STATUS_LOADING	(*(vu32*)(0x90004100))
#define STATUS_SECTOR	(*(vu32*)(0x90004100 + 8))
#define STATUS_DRIVE	(*(float*)(0x9000410C))
#define STATUS_GB_MB	(*(vu32*)(0x90004100 + 16))
#define STATUS_ERROR	(*(vu32*)(0x90004100 + 20))

static GXRModeObj *vmode = NULL;

static const unsigned char Boot2Patch[] =
{
    0x48, 0x03, 0x49, 0x04, 0x47, 0x78, 0x46, 0xC0, 0xE6, 0x00, 0x08, 0x70, 0xE1, 0x2F, 0xFF, 0x1E, 
    0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x0D, 0x25,
};
static const unsigned char FSAccessPattern[] =
{
    0x9B, 0x05, 0x40, 0x03, 0x99, 0x05, 0x42, 0x8B, 
};
static const unsigned char FSAccessPatch[] =
{
    0x9B, 0x05, 0x40, 0x03, 0x1C, 0x0B, 0x42, 0x8B, 
};

s32 __IOS_LoadStartupIOS(void)
{
	int res;

	res = __ES_Init();
	if(res < 0)
		return res;

	return 0;
}

u32 entrypoint = 0;
char launch_dir[MAXPATHLEN] = {0};
extern void __exception_closeall();
extern void udelay(u32 us);
static u8 loader_stub[0x1800]; //save internally to prevent overwriting
static ioctlv IOCTL_Buf ALIGNED(32);
static const char ARGSBOOT_STR[9] ALIGNED(0x10) = {'a','r','g','s','b','o','o','t','\0'}; //makes it easier to go through the file
int main(int argc, char **argv)
{
	// Exit after 10 seconds if there is an error
	__exception_setreload(10);
//	u64 timeout = 0;
	CheckForGecko();
	DCInvalidateRange(loader_stub, 0x1800);
	memcpy(loader_stub, (void*)0x80001800, 0x1800);
	DCFlushRange(loader_stub, 0x1800);

	u32 currev = *(vu32*)0x80003140;
	RAMInit();

	STM_RegisterEventHandler(HandleSTMEvent);

	Initialise();

	char* first_slash = strrchr(argv[0], '/');
	if (first_slash != NULL) strncpy(launch_dir, argv[0], first_slash-argv[0]+1);
	gprintf("launch_dir = %s\r\n", launch_dir);
	
	FPAD_Init();

	PrintInfo();
	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + + 430, MENU_POS_Y + 20*1, "Home: Exit");
	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + + 430, MENU_POS_Y + 20*2, "A   : Select");
	GRRLIB_Render();
	GRRLIB_DrawImg(0, 0, background, 0, 1, 1, 0xFFFFFFFF);

	if( *(vu32*)(0xCd800064) != -1 )
	{
		ClearScreen();
		gprintf("Please load Nintendont with AHBProt disabled!\r\n");
		PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, 232, "Please load Nintendont with AHBProt disabled!" );
		ExitToLoader(1);
	}

	u8 fw = *(vu8*)0x80003142;
	if( *(vu16*)0x80003140 != 58 || (IsWiiU() ? (fw != 25) : (fw != 24 && fw != 25)) || *(vu8*)0x80003143 != 32 )
	{
		ClearScreen();
		gprintf("This version of IOS58 is not supported!\r\n");
		PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, 232, "This version of IOS58 is not supported!" );
		ExitToLoader(1);
	}

	/* Read IPL Font before doing any patches */
	void *fontbuffer = memalign(32, 0x50000);
	__SYS_ReadROM((void*)fontbuffer,0x50000,0x1AFF00);
	memcpy((void*)0xD3100000, fontbuffer, 0x50000);
	DCInvalidateRange( (void*)0x93100000, 0x50000 );
	free(fontbuffer);
	//gprintf("Font: 0x1AFF00 starts with %.4s, 0x1FCF00 with %.4s\n", (char*)0x93100000, (char*)0x93100000 + 0x4D000);

	write16(0xD8B420A, 0); //disable MEMPROT for patches

//Patch FS access

	int u;
	for( u = 0x93A00000; u < 0x94000000; u+=2 )
	{
		if( memcmp( (void*)(u), FSAccessPattern, sizeof(FSAccessPattern) ) == 0 )
		{
		//	gprintf("FSAccessPatch:%08X\r\n", u );
			memcpy( (void*)u, FSAccessPatch, sizeof(FSAccessPatch) );
		}
	}

	fatInitDefault();
	
	// Simple code to autoupdate the meta.xml in Nintendont's folder
	FILE *meta = fopen("meta.xml", "w");
	if(meta != NULL)
	{
		fprintf(meta, "%s\r\n<app version=\"1\">\r\n\t<name>%s</name>\r\n", META_XML, META_NAME);
		fprintf(meta, "\t<coder>%s</coder>\r\n\t<version>%d.%d</version>\r\n", META_AUTHOR, NIN_VERSION>>16, NIN_VERSION&0xFFFF);
		fprintf(meta, "\t<release_date>20140430000000</release_date>\r\n");
		fprintf(meta, "\t<short_description>%s</short_description>\r\n", META_SHORT);
		fprintf(meta, "\t<long_description>%s\r\n\r\n%s</long_description>\r\n", META_LONG1, META_LONG2);
		fprintf(meta, "\t<ahb_access/>\r\n</app>");
		fclose(meta);
	}
	LoadTitles();

	gprintf("Nintendont Loader\r\n");
	gprintf("Built   : %s %s\r\n", __DATE__, __TIME__ );
	gprintf("Version : %d.%d\r\n", NIN_VERSION>>16, NIN_VERSION&0xFFFF );
	gprintf("Firmware: %d.%d.%d\r\n", *(vu16*)0x80003140, *(vu8*)0x80003142, *(vu8*)0x80003143 );

	memset((void*)ncfg, 0, sizeof(NIN_CFG));

	bool argsboot = false;
	if(argc > 1) //every 0x00 gets counted as one arg so just make sure its more than the path and copy
	{
		memcpy(ncfg, argv[1], sizeof(NIN_CFG));
		if (ncfg->Version == 2)	//need to upgrade config from ver 2 to ver 3
		{
			ncfg->MemCardBlocks = 0x2;//251 blocks
			ncfg->Version = 3;
		}
		if(ncfg->Magicbytes == 0x01070CF6 && ncfg->Version == NIN_CFG_VERSION && ncfg->MaxPads <= NIN_CFG_MAXPAD)
		{
			if(ncfg->Config & NIN_CFG_AUTO_BOOT)
			{	//do NOT remove, this can be used to see if nintendont knows args
				gprintf(ARGSBOOT_STR);
				argsboot = true;
			}
		}
	}
	if(argsboot == false)
	{
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
				GRRLIB_DrawImg(0, 0, background, 0, 1, 1, 0xFFFFFFFF);
			}
			
			FPAD_Update();

			if (FPAD_Cancel(0)) {
				ncfg->Config &= ~NIN_CFG_AUTO_BOOT;
				break;
			}

			i++;
		}
	}
	UseSD = (ncfg->Config & NIN_CFG_USB) == 0;

	bool progressive = (CONF_GetProgressiveScan() > 0) && VIDEO_HaveComponentCable();
	if(progressive) //important to prevent blackscreens
		ncfg->VideoMode |= NIN_VID_PROG;
	else
		ncfg->VideoMode &= ~NIN_VID_PROG;


	if((ncfg->Config & NIN_CFG_AUTO_BOOT) == 0)
	{
		while (1)
		{
			UseSD = (ncfg->Config & NIN_CFG_USB) == 0;
			PrintInfo();
			PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 430, MENU_POS_Y + 20*1, "Home: Exit");
			PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 430, MENU_POS_Y + 20*2, "A   : Select");
			PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 53 * 6 - 8, MENU_POS_Y + 20 * 6, UseSD ? "<" : " ");
			PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 53 * 6 - 8, MENU_POS_Y + 20 * 7, UseSD ? " " : "<");
			PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 47 * 6 - 8, MENU_POS_Y + 20 * 6, " SD  ");
			PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 47 * 6 - 8, MENU_POS_Y + 20 * 7, "USB  ");
			FPAD_Update();

			if (FPAD_OK(0))
				break;

			if (FPAD_Start(1))
			{
				ClearScreen();
				PrintFormat(DEFAULT_SIZE, BLACK, 212, 232, "Returning to loader...");
				ExitToLoader(0);
			}
			if (FPAD_Down(0))
				ncfg->Config = ncfg->Config | NIN_CFG_USB;

			if (FPAD_Up(0))
				ncfg->Config = ncfg->Config & ~NIN_CFG_USB;

			GRRLIB_Render();
			GRRLIB_DrawImg(0, 0, background, 0, 1, 1, 0xFFFFFFFF);
		}
		ClearScreen();
		PrintFormat(DEFAULT_SIZE, BLACK, 212, 232, "Loading, please wait...");
		GRRLIB_Render();
		GRRLIB_DrawImg(0, 0, background, 0, 1, 1, 0xFFFFFFFF);
	}

	if(LoadKernel() < 0)
	{
		ClearScreen();
		gprintf("Failed to load kernel from NAND!\r\n");
		PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, 232, "Failed to load kernel from NAND!" );
		ExitToLoader(1);
	}
	InsertModule((char*)kernel_bin, kernel_bin_size);

	memset( (void*)0x92f00000, 0, 0x100000 );
	DCFlushRange( (void*)0x92f00000, 0x100000 );

	/*FILE *out = fopen("/kernel.bin", "wb");
	fwrite( (char*)0x90100000, 1, NKernelSize, out );
	fclose(out);*/

//Load config

//Reset drive

	if( ncfg->Config & NIN_CFG_AUTO_BOOT )
		gprintf("Autobooting:\"%s\"\r\n", ncfg->GamePath );
	else
		SelectGame();

	//Set Language
	if(ncfg->Language == NIN_LAN_AUTO)
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

//setup memory card
	if(ncfg->Config & NIN_CFG_MEMCARDEMU)
	{
		char BasePath[20];
		sprintf(BasePath, "%s:/saves", GetRootDevice());
		mkdir(BasePath, S_IREAD | S_IWRITE);
		char MemCardName[8];
		memset(MemCardName, 0, 8);
		if ( ncfg->Config & NIN_CFG_MC_MULTI )
		{
			if ((ncfg->GameID & 0xFF) == 'J')  // JPN games
				memcpy(MemCardName, "ninmemj", 7);
			else
				memcpy(MemCardName, "ninmem", 6);
		}
		else
			memcpy(MemCardName, &(ncfg->GameID), 4);
		char MemCard[30];
		sprintf(MemCard, "%s/%s.raw", BasePath, MemCardName);
		gprintf("Using %s as Memory Card.\r\n", MemCard);
		FILE *f = fopen(MemCard, "rb");
		if(f == NULL)
		{
			f = fopen(MemCard, "wb");
			if(f == NULL)
			{
				ClearScreen();
				PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, 232, "Failed to create Memory Card File!" );
				ExitToLoader(1);
			}
			char NullChar[1];
			NullChar[0] = 0;
			fwrite(NullChar, 1, MEM_CARD_SIZE(ncfg->MemCardBlocks), f);
			gprintf("Memory Card File created!\r\n");
		}
		if(f != NULL)
			fclose(f);
	}
//sync changes
	CloseDevices();

	WPAD_Disconnect(0);
	WPAD_Disconnect(1);
	WPAD_Disconnect(2);
	WPAD_Disconnect(3);

	WUPC_Shutdown();
	WPAD_Shutdown();

	//for BT.c
	CONF_GetPadDevices((conf_pads*)0x932C0000);
	DCFlushRange((void*)0x932C0000, sizeof(conf_pads));

	DCInvalidateRange( (void*)0x939F02F0, 0x20 );

	memcpy( (void*)0x939F02F0, Boot2Patch, sizeof(Boot2Patch) );

	DCFlushRange( (void*)0x939F02F0, 0x20 );

	s32 fd = IOS_Open( "/dev/es", 0 );

	memset( STATUS, 0xFFFFFFFF, 0x20  );
	DCFlushRange( STATUS, 0x20 );

	memset( (void*)0x91000000, 0xFFFFFFFF, 0x20  );
	DCFlushRange( (void*)0x91000000, 0x20 );

	//make sure the cfg gets to the kernel
	DCFlushRange((void*)ncfg, sizeof(NIN_CFG));

	/* dont directly reset the kernel */
	DCInvalidateRange( (void*)0x9300300C, 0x20 );
	*(vu32*)0x9300300C = 0;
	DCFlushRange( (void*)0x9300300C, 0x20 );

	gprintf("ES_ImportBoot():");

	write32(0x80003140, 0);
	__MaskIrq(IRQ_PI_ACR);
	raw_irq_handler_t irq_handler = IRQ_Free(IRQ_PI_ACR);

	u32 ret = IOS_IoctlvAsync( fd, 0x1F, 0, 0, &IOCTL_Buf, NULL, NULL );
	gprintf("%d\r\n", ret );
	gprintf("Waiting ...\r\n");

	while((read32(0x80003140)) != 0x00000D25)
		udelay(1000);
	u32 counter;
	for (counter = 0; !(read32(0x0d000004) & 2); counter++)
	{
		udelay(1000);
		if (counter >= 40000)
			break;
	}
	gprintf("IPC started (%u)\r\n", counter);
	IRQ_Request(IRQ_PI_ACR, irq_handler, NULL);
	__UnmaskIrq(IRQ_PI_ACR);
	__IPC_Reinitialize();

	while(1)
	{
		DCInvalidateRange( STATUS, 0x20 );
		if( STATUS_LOADING == 0xdeadbeef )
			break;
		
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
			PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*8, "Init USB/SD device...");
		if(abs(STATUS_LOADING) > 2 && abs(STATUS_LOADING) < 20)
			PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*8, "Init USB/SD device... Done!");
		if(STATUS_LOADING == -2)
			PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*8, "Init USB/SD device... Error! %d  Shutting down", STATUS_ERROR);
		if(STATUS_LOADING == 3)
			PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*9, "Mounting USB/SD device...");
		if(abs(STATUS_LOADING) > 3 && abs(STATUS_LOADING) < 20)
			PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*9, "Mounting USB/SD device... Done!");
		if(STATUS_LOADING == -3)
			PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*9, "Mounting USB/SD device... Error! %d  Shutting down", STATUS_ERROR);
		if(STATUS_LOADING == 5) {
/*			if (timeout == 0)
				timeout = ticks_to_secs(gettime()) + 20; // Set timer for 20 seconds
			else if (timeout <= ticks_to_secs(gettime())) {
				STATUS_ERROR = -7;
				DCFlushRange(STATUS, 0x20);
				usleep(100);
				//memset( (void*)0x92f00000, 0, 0x100000 );
				//DCFlushRange( (void*)0x92f00000, 0x100000 );
				//ExitToLoader(1);
			}
*/
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
		if(STATUS_LOADING == 8)
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
		}
		if(STATUS_LOADING == 9)
			PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*15, "Init DI... %40s", " ");
		if(abs(STATUS_LOADING) > 9 && abs(STATUS_LOADING) < 20)
			PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*15, "Init DI... Done! %35s", " ");
		if(STATUS_LOADING == 10)
			PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*16, "Init CARD...");
		if(abs(STATUS_LOADING) > 10 && abs(STATUS_LOADING) < 20)
			PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*16, "Init CARD... Done!");
		if(STATUS_LOADING == -10)
		{
			PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*16, "Init CARD... Failed! Shutting down");
			switch (STATUS_ERROR)
			{
				case -1:
					PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*17, "Missing %s:/sneek/kenobiwii.bin", GetRootDevice());
					break;	
				case -2:
					if (ncfg->Config & NIN_CFG_CHEAT_PATH)
						PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*17, "Missing %s:/%s", GetRootDevice(), ncfg->CheatPath);
					else
//						PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*17, "Missing %s:/games/%.6s/%.6s.gct", GetRootDevice(), ncfg->GameID, ncfg->GameID);
						PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*17, "Missing %s:/games/GAMEID/GAMEID.gct", GetRootDevice());
					break;	
				case -3:
					PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*17, "Cheat file to large", GetRootDevice());
					break;	
				default:
					PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*17, "Unknown error %d %35s", STATUS_ERROR, " ");
					break;	
			}
		}
		GRRLIB_Screen2Texture(0, 0, screen_buffer, GX_FALSE); // Copy all status messages
		GRRLIB_Render();
		GRRLIB_DrawImg(0, 0, background, 0, 1, 1, 0xFFFFFFFF);
		while ((STATUS_LOADING < -1) && (STATUS_LOADING > -20))	//displaying a fatal error
			{ }	//do nothing wait for shutdown
	}

	gprintf("Nintendont at your service!\r\n");

	GRRLIB_DrawImg(0, 0, screen_buffer, 0, 1, 1, 0xFFFFFFFF); // Draw all status messages
	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*17, "Nintendont kernel looping, loading game...");
	GRRLIB_Render();
	GRRLIB_DrawImg(0, 0, screen_buffer, 0, 1, 1, 0xFFFFFFFF); // Draw all status messages
//	memcpy( (void*)0x80000000, (void*)0x90140000, 0x1200000 );
	DVDStartCache();
	GRRLIB_FreeTexture(background);
	GRRLIB_FreeTexture(screen_buffer);
	GRRLIB_FreeTTF(myFont);
	GRRLIB_Exit();
	

	gprintf("GameRegion:");

	if( ncfg->VideoMode & NIN_VID_FORCE )
	{
		gprintf("Force:%02X\r\n", ncfg->VideoMode & NIN_VID_FORCE_MASK );

		switch( ncfg->VideoMode & NIN_VID_FORCE_MASK )
		{
			case NIN_VID_FORCE_NTSC:
			{
				*(vu32*)0x800000CC = 0;
				Region = 0;
			} break;
			case NIN_VID_FORCE_MPAL:
			{
				*(vu32*)0x800000CC = 3;
				Region = 2;
			} break;
			case NIN_VID_FORCE_PAL50:
			{
				*(vu32*)0x800000CC = 1;
				Region = 2;
			} break;
			case NIN_VID_FORCE_PAL60:
			{
				*(vu32*)0x800000CC = 5;
				Region = 2;
			} break;
		}
	}
	else
	{
		switch (ncfg->GameID & 0x000000FF)
		{
			// EUR
			case 'D':
			case 'F':
			case 'H':
			case 'I':
			case 'M':
			case 'P':
			case 'S':
			case 'U':
			case 'X':
			case 'Y':
			case 'Z':
				Region = 2;
				break;
			// JP and US
			case 'J':
			case 'E':
			default:
				Region = 0;
				break;
		}
	}
	
	gprintf("Region:%u\r\n", Region );
	progressive = ncfg->Config & NIN_CFG_FORCE_PROG;
	switch(Region)
	{
		default:
		case 0:
		case 1:
		{
			gprintf("NTSC\r\n");

			*(vu32*)0x800000CC = 0;

			if(progressive)
				vmode = &TVNtsc480Prog;
			else
				vmode = &TVNtsc480IntDf;
			
		} break;
		case 2:
		{
			if( *(vu32*)0x800000CC == 5 )
			{
				gprintf("PAL60\r\n");

				if(progressive)
					vmode = &TVNtsc480Prog;
				else
					vmode = &TVEurgb60Hz480IntDf;

			} else if( *(vu32*)0x800000CC == 3 ) {
				gprintf("MPAL\r\n");

				if(progressive)
					vmode = &TVNtsc480Prog;
				else
					vmode = &TVMpal480IntDf;
			} else {
				
				gprintf("PAL50\r\n");

				if(progressive)
					vmode = &TVEurgb60Hz480Prog;
				else
					vmode = &TVPal528IntDf;
			}

			*(vu32*)0x800000CC = 1;

		} break;
	}
	VIDEO_Configure( vmode );
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

	// set current time
	u32 bias = 0, cur_time = 0;
	__SYS_GetRTC(&cur_time);
	if(CONF_GetCounterBias(&bias) >= 0)
		cur_time += bias;
	settime(secs_to_ticks(cur_time));

	DCInvalidateRange((void*)0x93000000, 0x3000);
	memcpy((void*)0x93000000, PADReadGC_bin, PADReadGC_bin_size);
	memset((void*)0x93002700, 0, 0x200); //clears alot of pad stuff
	memset((void*)0x93002C00, 0, 0x400); //clears alot of multidol stuff
	//strcpy((char*)0x930028A0, "ARStartDMA: %08x %08x %08x\n"); //ARStartDMA Debug
	DCFlushRange((void*)0x93000000, 0x3000);

	DCInvalidateRange((void*)0x93010010, 0x10000);
	memcpy((void*)0x93010010, loader_stub, 0x1800);
	memcpy((void*)0x93011810, stub_bin, stub_bin_size);
	DCFlushRange((void*)0x93010010, 0x10000);

	DCInvalidateRange((void*)0x93020000, 0x10000);
	memset((void*)0x93020000, 0, 0x10000);
	DCFlushRange((void*)0x93020000, 0x10000);

	DCInvalidateRange((void*)0x93003000, 0x20);
	*(vu32*)0x93003000 = currev; //set kernel rev
	*(vu32*)0x93003008 = 0x80000004; //just some address for SIGetType
	//*(vu32*)0x9300300C = 3; //init cache if needed
	memset((void*)0x93003010, 0, 0x10); //disable rumble on bootup
	DCFlushRange((void*)0x93003000, 0x20);

	/*while(*(vu32*)0x9300300C == 3)
	{
		DCFlushRange((void*)0x9300300C, 4);
		usleep(500);
	}*/
	write16(0xD8B420A, 0); //disable MEMPROT again after reload
	//u32 level = IRQ_Disable();
	__exception_closeall();
	__lwp_thread_closeall();

	memcpy((void*)0x81300000, multidol_ldr_bin, multidol_ldr_bin_size);
	DCFlushRange((void*)0x81300000, multidol_ldr_bin_size);
	ICInvalidateRange((void*)0x81300000, multidol_ldr_bin_size);

	asm volatile (
		"lis %r3, 0x8130\n"
		"mtlr %r3\n"
		"blr\n"
	);
	//IRQ_Restore(level);

	return 0;
}

