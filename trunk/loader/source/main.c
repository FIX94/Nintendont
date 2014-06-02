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
#include "loader.h"
#include "Patches.h"
#include "kernel_bin.h"
#include "kernel_usb_bin.h"
#include "PADReadGC_bin.h"
#include "PADReadHID_bin.h"
#include "stub_bin.h"

extern void __exception_setreload(int t);
u32 __SYS_GetRTC(u32 *gctime);

#define STATUS_LOADING	(*(vu32*)(0x90004100))
#define STATUS_SECTOR	(*(vu32*)(0x90004100 + 8))
#define STATUS_DRIVE	(*(vu32*)(0x90004100 + 12))
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
extern void __exception_closeall();
extern void udelay(u32 us);
static u8 loader_stub[0x1800]; //save internally to prevent overwriting
static ioctlv IOCTL_Buf __attribute__((aligned(32)));
int main(int argc, char **argv)
{
	// Exit after 10 seconds if there is an error
	__exception_setreload(10);
	CheckForGecko();
	DCInvalidateRange(loader_stub, 0x1800);
	memcpy(loader_stub, (void*)0x80001800, 0x1800);
	DCFlushRange(loader_stub, 0x1800);

	if( !IsWiiU() )
	{
		gprintf("Nintendont Loader\r\n");
		gprintf("Built   : %s %s\r\n", __DATE__, __TIME__ );
		gprintf("Version : %d.%d\r\n", NIN_VERSION>>16, NIN_VERSION&0xFFFF );	
	}
	u32 currev = *(vu32*)0x80003140;
	HollywoodRevision = SYS_GetHollywoodRevision();	//RAMInit overwrites this
	RAMInit();

	STM_RegisterEventHandler(HandleSTMEvent);

	Initialise();

	FPAD_Init();

	PrintInfo();
	PrintFormat( MENU_POS_X + 44 * 5, MENU_POS_Y + 20*1, "Home: Exit");
	PrintFormat( MENU_POS_X + 44 * 5, MENU_POS_Y + 20*2, "A   : Select");
	
	if( *(vu32*)(0xCd800064) != -1 )
	{
		ClearScreen();
		gprintf("Please load Nintendont with AHBProt disabled!\r\n");
		PrintFormat( 25, 232, "Please load Nintendont with AHBProt disabled!" );
		ExitToLoader(1);
	}

	int fw = IsWiiU() ? 25 : 24;
	if( *(vu16*)0x80003140 != 58 || *(vu8*)0x80003142 != fw || *(vu8*)0x80003143 != 32 )
	{
		ClearScreen();
		gprintf("This version of IOS58 is not supported!\r\n");
		PrintFormat( 25, 232, "This version of IOS58 is not supported!" );
		ExitToLoader(1);
	}
	
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

	if( IsWiiU() )
	{
		gprintf("Built   : %s %s\r\n", __DATE__, __TIME__ );
		gprintf("Version : %d.%d\r\n", NIN_VERSION>>16, NIN_VERSION&0xFFFF );	
		gprintf("Firmware: %d.%d.%d\r\n", *(vu16*)0x80003140, *(vu8*)0x80003142, *(vu8*)0x80003143 );
	}
	
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
	
	u32 ConfigReset = 0;

	cfg = fopen("/nincfg.bin", "rb+");
	if (cfg == NULL)
	{
		ConfigReset = 1;

	}
	else {

		if (fread(&ncfg, sizeof(NIN_CFG), 1, cfg) != 1)
			ConfigReset = 1;

		if (ncfg.Magicbytes != 0x01070CF6)
			ConfigReset = 1;

		if (ncfg.Version != NIN_CFG_VERSION)
			ConfigReset = 1;

		if (ncfg.MaxPads > NIN_CFG_MAXPAD)
			ConfigReset = 1;

		if (ncfg.MaxPads < 1)
			ConfigReset = 1;

		fclose(cfg);
	}

	// Prevent autobooting if B is pressed
	int i = 0;
	while((ncfg.Config & NIN_CFG_AUTO_BOOT) && i < 100000) // wait for wiimote re-synch
	{
		PrintFormat( MENU_POS_X + 44 * 5, MENU_POS_Y + 20*3, "B   : Stop Boot");
		FPAD_Update();

		if (FPAD_Cancel(0))
		{
			ncfg.Config &= ~NIN_CFG_AUTO_BOOT;
		}
		i++;
	}
	
	if (ConfigReset)
	{
		memset(&ncfg, 0, sizeof(NIN_CFG));

		ncfg.Magicbytes = 0x01070CF6;
		ncfg.Version = NIN_CFG_VERSION;
		ncfg.Language = NIN_LAN_AUTO;
		ncfg.MaxPads = NIN_CFG_MAXPAD;
	}
	bool progressive = (CONF_GetProgressiveScan() > 0) && VIDEO_HaveComponentCable();
	if(progressive) //important to prevent blackscreens
		ncfg.VideoMode |= NIN_VID_PROG;
	else
		ncfg.VideoMode &= ~NIN_VID_PROG;

	PrintFormat(MENU_POS_X + 47 * 6 - 8, MENU_POS_Y + 20 * 6, " SD  ");
	PrintFormat(MENU_POS_X + 47 * 6 - 8, MENU_POS_Y + 20 * 7, "USB  ");
	bool QueryUser = (ncfg.Config & NIN_CFG_AUTO_BOOT) == 0;
	UseSD = (ncfg.Config & NIN_CFG_USB) == 0;
	while (QueryUser)
	{
		UseSD = (ncfg.Config & NIN_CFG_USB) == 0;
		PrintFormat(MENU_POS_X + 51 * 6 - 8, MENU_POS_Y + 20 * 6, UseSD ? "<" : " ");
		PrintFormat(MENU_POS_X + 51 * 6 - 8, MENU_POS_Y + 20 * 7, UseSD ? " " : "<");

		FPAD_Update();

		if (FPAD_OK(0))
		{
			int pos = UseSD ? 6 : 7;
			PrintFormat(MENU_POS_X + 51 * 6 - 8, MENU_POS_Y + 20 * pos, " ");
			break;
		}
		if (FPAD_Start(1))
		{
			ClearScreen();
			PrintFormat(90, 232, "Returning to loader...");
			ExitToLoader(0);
		}
		if (FPAD_Down(0))
		{
			ncfg.Config = ncfg.Config | NIN_CFG_USB;
		}
		if (FPAD_Up(0))
		{
			ncfg.Config = ncfg.Config & ~NIN_CFG_USB;
		}
	}

	u32 KernelSize = 0;
	u32 NKernelSize = 0;
	char *Kernel = (char*)0x80100000;

	if( LoadKernel( Kernel, &KernelSize ) < 0 )
	{
		ClearScreen();
		gprintf("Failed to load kernel from NAND!\r\n");
		PrintFormat( 25, 232, "Failed to load kernel from NAND!" );
		ExitToLoader(1);
	}

	if (UseSD)
		InsertModule( Kernel, KernelSize, (char*)kernel_bin, kernel_bin_size, (char*)0x90100000, &NKernelSize );
	else
		InsertModule(Kernel, KernelSize, (char*)kernel_usb_bin, kernel_usb_bin_size, (char*)0x90100000, &NKernelSize);

	DCFlushRange( (void*)0x90100000, NKernelSize );

	/*FILE *out = fopen("/kernel.bin", "wb");
	fwrite( (char*)0x90100000, 1, NKernelSize, out );
	fclose(out);*/

//Load config

//Reset drive

	if( ncfg.Config & NIN_CFG_AUTO_BOOT )
	{
		gprintf("Autobooting:\"%s\"\r\n", ncfg.GamePath );
	} else {
		SelectGame();
	}

//setup memory card
	if(ncfg.Config & NIN_CFG_MEMCARDEMU)
	{
		char BasePath[20];
		sprintf(BasePath, "%s:/saves", GetRootDevice());
		mkdir(BasePath, S_IREAD | S_IWRITE);
		char MemCardName[5];
		memset(MemCardName, 0, 5);
		memcpy(MemCardName, &ncfg.GameID, 4);
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
				PrintFormat( 25, 232, "Failed to create Memory Card File!" );
				ExitToLoader(1);
			}
			char NullChar[1];
			NullChar[0] = 0;
			fwrite(NullChar, 1, NIN_RAW_MEMCARD_SIZE, f);
			gprintf("Memory Card File created!\r\n");
		}
		if(f != NULL)
			fclose(f);
	}
//sync changes
	fatUnmount(GetRootDevice());
	ClearScreen();
	PrintInfo();

	WPAD_Disconnect(0);
	WPAD_Disconnect(1);
	WPAD_Disconnect(2);
	WPAD_Disconnect(3);

	WPAD_Shutdown();

	DCInvalidateRange( (void*)0x939F02F0, 0x20 );

	memcpy( (void*)0x939F02F0, Boot2Patch, sizeof(Boot2Patch) );

	DCFlushRange( (void*)0x939F02F0, 0x20 );

	s32 fd = IOS_Open( "/dev/es", 0 );

	memset( (void*)0x90004100, 0xFFFFFFFF, 0x20  );
	DCFlushRange( (void*)0x90004100, 0x20 );

	memset( (void*)0x91000000, 0xFFFFFFFF, 0x20  );
	DCFlushRange( (void*)0x91000000, 0x20 );

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

	PrintFormat( MENU_POS_X, MENU_POS_Y + 20*6, "Loading patched kernel ...\r\n");
	while(1)
	{
		DCInvalidateRange( (void*)0x90004100, 0x20 );
		if( STATUS_LOADING == 0xdeadbeef )
			break;

		PrintFormat(MENU_POS_X, MENU_POS_Y + 20*6, "Loading patched kernel... %d", STATUS_LOADING);
		if(STATUS_LOADING == 0)
		{
			PrintFormat(MENU_POS_X, MENU_POS_Y + 20*7, "ES_Init...");
		// Cleans the -1 when it's past it to avoid confusion if another error happens. e.g. before it showed "81" instead of "8" if the controller was unplugged.
			PrintFormat(MENU_POS_X + 163, MENU_POS_Y + 20*6, " ");
		}
		if(STATUS_LOADING > 0 && STATUS_LOADING < 20)
			PrintFormat(MENU_POS_X, MENU_POS_Y + 20*7, "ES_Init... Done!");
		if(STATUS_LOADING == 2)
			PrintFormat(MENU_POS_X, MENU_POS_Y + 20*8, "Init SD device...");
		if(STATUS_LOADING > 2 && STATUS_LOADING < 20)
			PrintFormat(MENU_POS_X, MENU_POS_Y + 20*8, "Init SD device... Done!");
		if(STATUS_LOADING == -2)
			PrintFormat(MENU_POS_X, MENU_POS_Y + 20*8, "Init SD device... Error! %d  Shutting down", STATUS_ERROR);
		if(STATUS_LOADING == 3)
			PrintFormat(MENU_POS_X, MENU_POS_Y + 20*9, "Mounting USB/SD device...");
		if(STATUS_LOADING > 3 && STATUS_LOADING < 20)
			PrintFormat(MENU_POS_X, MENU_POS_Y + 20*9, "Mounting USB/SD device... Done!");
		if(STATUS_LOADING == -3)
			PrintFormat(MENU_POS_X, MENU_POS_Y + 20*9, "Mounting USB/SD device... Error! %d  Shutting down", STATUS_ERROR);
		if(STATUS_LOADING == 5)
			PrintFormat(MENU_POS_X, MENU_POS_Y + 20*10, "Checking FS...");
		if(STATUS_LOADING > 5 && STATUS_LOADING < 20)
		{
			PrintFormat( MENU_POS_X, MENU_POS_Y + 20*10, "Checking FS... Done!");
			PrintFormat( MENU_POS_X, MENU_POS_Y + 20*11, "Drive size: %d%s Sector size: %d", STATUS_DRIVE, STATUS_GB_MB ? "GB" : "MB", STATUS_SECTOR);
		}
		if(STATUS_LOADING == -5)
			PrintFormat(MENU_POS_X, MENU_POS_Y + 20*10, "Checking FS... Error! %d Shutting down", STATUS_ERROR);
		if(STATUS_LOADING == 6)
			PrintFormat(MENU_POS_X, MENU_POS_Y + 20*12, "ES_LoadModules...");
		if(STATUS_LOADING > 6 && STATUS_LOADING < 20)
			PrintFormat(MENU_POS_X, MENU_POS_Y + 20*12, "ES_LoadModules... Done!");
		if(STATUS_LOADING == -6)
			PrintFormat(MENU_POS_X, MENU_POS_Y + 20*12, "ES_LoadModules... Error! %d Shutting down", STATUS_ERROR);
		if(STATUS_LOADING == 7)
			PrintFormat(MENU_POS_X, MENU_POS_Y + 20*13, "Loading config...");
		if(STATUS_LOADING > 7 && STATUS_LOADING < 20)
			PrintFormat(MENU_POS_X, MENU_POS_Y + 20*13, "Loading config... Done!");
		if(STATUS_LOADING == 8)
		{
			PrintFormat(MENU_POS_X, MENU_POS_Y + 20*14, "Init HID devices... ");
			if ( STATUS_ERROR == 1)
			{
				PrintFormat(MENU_POS_X, MENU_POS_Y + 20*15, "          Plug Controller in %s usb port", IsWiiU() ? "BOTTOM REAR" : "TOP");
			}
			else
				PrintFormat(MENU_POS_X, MENU_POS_Y + 20*15, "%50s", " ");
		}
		if(STATUS_LOADING > 8 && STATUS_LOADING < 20)
		{
			if (ncfg.Config & NIN_CFG_HID)
				PrintFormat(MENU_POS_X, MENU_POS_Y + 20*14, "Init HID devices... Done!");
			else
				PrintFormat(MENU_POS_X, MENU_POS_Y + 20*14, "Init HID devices... Using Gamecube Ports... Done!");
		}
		if(STATUS_LOADING == -8)
		{
			PrintFormat(MENU_POS_X, MENU_POS_Y + 20*14, "Init HID devices... Failed! Shutting down");
			switch (STATUS_ERROR)
			{
				case -1:
					PrintFormat(MENU_POS_X, MENU_POS_Y + 20*15, "No Controller plugged in %s usb port %10s", IsWiiU() ? "BOTTOM REAR" : "TOP", " ");
					break;	
				case -2:
					PrintFormat(MENU_POS_X, MENU_POS_Y + 20*15, "Missing %s:/controller.ini %20s", GetRootDevice(), " ");
					break;	
				case -3:
					PrintFormat(MENU_POS_X, MENU_POS_Y + 20*15, "Controller does not match %s:/controller.ini %6s", GetRootDevice(), " ");
					break;	
				case -4:
					PrintFormat(MENU_POS_X, MENU_POS_Y + 20*15, "Invalid Polltype in %s:/controller.ini %12s", GetRootDevice(), " ");
					break;	
				case -5:
					PrintFormat(MENU_POS_X, MENU_POS_Y + 20*15, "Invalid DPAD value in %s:/controller.ini %9s", GetRootDevice(), " ");
					break;	
				case -6:
					PrintFormat(MENU_POS_X, MENU_POS_Y + 20*15, "PS3 controller init error %25s", " ");
					break;	
				default:
					PrintFormat(MENU_POS_X, MENU_POS_Y + 20*15, "Unknown error %d %35s", STATUS_ERROR, " ");
					break;	
			}
		}
		if(STATUS_LOADING == 9)
			PrintFormat(MENU_POS_X, MENU_POS_Y + 20*15, "Init DI... %40s", " ");
		if(STATUS_LOADING > 9 && STATUS_LOADING < 20)
			PrintFormat(MENU_POS_X, MENU_POS_Y + 20*15, "Init DI... Done! %35s", " ");
		if(STATUS_LOADING == 10)
			PrintFormat(MENU_POS_X, MENU_POS_Y + 20*16, "Init CARD...");
		if(STATUS_LOADING > 10 && STATUS_LOADING < 20)
			PrintFormat(MENU_POS_X, MENU_POS_Y + 20*16, "Init CARD... Done!");
		VIDEO_WaitVSync();
	}

	if( !IsWiiU() )
		gprintf("Nintendont at your service!\r\n");

	PrintFormat( MENU_POS_X, MENU_POS_Y + 20*17, "Nintendont kernel looping, loading game...");
//	memcpy( (void*)0x80000000, (void*)0x90140000, 0x1200000 );
	entrypoint = LoadGame();

	gprintf("GameRegion:");

	if( ncfg.VideoMode & NIN_VID_FORCE )
	{
		gprintf("Force:%u (%02X)\r\n", ncfg.VideoMode & NIN_VID_FORCE, ncfg.VideoMode & NIN_VID_FORCE_MASK );

		switch( ncfg.VideoMode & NIN_VID_FORCE_MASK )
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
	
	gprintf("Region:%u\r\n", Region );

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
					vmode = &TVEurgb60Hz480Prog;
				else
					vmode = &TVEurgb60Hz480IntDf;

			} else if( *(vu32*)0x800000CC == 3 ) {
				gprintf("MPAL\r\n");

				if(progressive)
					vmode = &TVEurgb60Hz480Prog;
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
	ClearScreen();
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

	ICFlashInvalidate();

	DCInvalidateRange((void*)0x93000000, 0x3000);
	if(IsWiiU())
	{
		*(vu32*)0x93000000 = 0x4E800020; //blr, no gc controller on wiiu
		memcpy((void*)0x93001000, PADReadHID_bin, PADReadHID_bin_size);
	}
	else
	{
		memcpy((void*)0x93000000, PADReadGC_bin, PADReadGC_bin_size);
		if( ncfg.Config & NIN_CFG_HID )
			memcpy((void*)0x93001000, PADReadHID_bin, PADReadHID_bin_size);
		else
			*(vu32*)0x93001000 = 0x4E800020; //blr, no HID requested
	}
	memset((void*)0x93002700, 0, 4); //set HID controller to 0
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

	gprintf("entrypoint(0x%08X)\r\n", entrypoint );

	asm volatile (
		"lis %r3, entrypoint@h\n"
		"ori %r3, %r3, entrypoint@l\n"
		"lwz %r3, 0(%r3)\n"
		"mtlr %r3\n"
		"blr\n"
	);
	//IRQ_Restore(level);

	return 0;
}

