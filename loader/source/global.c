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

#include <fat.h>
#include <gccore.h>
#include <ogc/audio.h>
#include <ogc/consol.h>
#include <ogc/dsp.h>
#include <ogc/es.h>
#include <ogc/gx_struct.h>
#include <ogc/ipc.h>
#include <ogc/lwp_threads.h>
#include <ogc/system.h>
#include <ogc/video.h>
#include <ogc/wiilaunch.h>
#include <stdio.h>
#include <unistd.h>

#include "Config.h"
#include "exi.h"
#include "global.h"
#include "font_ttf.h"


GRRLIB_ttfFont *myFont;
GRRLIB_texImg *background;
GRRLIB_texImg *screen_buffer;

u32 Region;
u32 POffset;

NIN_CFG* ncfg = (NIN_CFG*)0x93002900;
bool UseSD;

inline bool IsWiiU( void )
{
	return ( (*(vu32*)(0xCd8005A0) >> 16 ) == 0xCAFE );
}
const char* const GetRootDevice()
{
	static const char* const SdStr = "sd";
	static const char* const UsbStr = "usb";
	if (UseSD)
		return SdStr;
	else
		return UsbStr;
}
void RAMInit(void)
{
	__asm("lis 3, 0x8390\n\
		  mtspr 0x3F3, 3");

	__asm("mfspr 3, 1008");
	__asm("ori 3, 3, 0x200");
	__asm("mtspr 1008, 3");

	__asm("mtfsb1    4*cr7+gt");

	memset( (void*)0x80000000, 0, 0x100 );
	memset( (void*)0x80003000, 0, 0x100 );
	//memset( (void*)0x80003F00, 0, 0x11FC100 );
	memset( (void*)0x81340000, 0, 0x3C );

	__asm("	isync\n\
	li      4, 0 \n\
	mtspr   541, 4\n\
	mtspr   540, 4 \n\
	mtspr   543, 4\n\
	mtspr   542, 4\n\
	mtspr   531, 4\n\
	mtspr   530, 4\n\
	mtspr   533, 4\n\
	mtspr   532, 4\n\
	mtspr   535, 4\n\
	mtspr   534, 4\n\
	isync");
	
	*(vu32*)0x80000028 = 0x01800000;
	*(vu32*)0x8000002C = 0;

	*(vu32*)0x8000002C = *(vu32*)0xCC00302C >> 28;		// console type
	*(vu32*)0x80000038 = 0x01800000;
	*(vu32*)0x800000F0 = 0x01800000;
	*(vu32*)0x800000EC = 0x81800000;
	
	*(vu32*)0x80003100 = 0x01800000;	//Physical MEM1 size 
	*(vu32*)0x80003104 = 0x01800000;	//Simulated MEM1 size 
	*(vu32*)0x80003108 = 0x01800000;
	*(vu32*)0x8000310C = 0;
	*(vu32*)0x80003110 = 0;
	*(vu32*)0x80003114 = 0;
	*(vu32*)0x80003118 = 0;				//Physical MEM2 size 
	*(vu32*)0x8000311C = 0;				//Simulated MEM2 size 
	*(vu32*)0x80003120 = 0;
	*(vu32*)0x80003124 = 0x0000FFFF;
	*(vu32*)0x80003128 = 0;
	*(vu32*)0x80003130 = 0x0000FFFF;	//IOS Heap Range 
	*(vu32*)0x80003134 = 0;
	*(vu32*)0x80003138 = 0x11;			//Hollywood Version 
	*(vu32*)0x8000313C = 0;

	*(vu32*)0x800030CC = 0;
	*(vu32*)0x800030C8 = 0;
	*(vu32*)0x800030D0 = 0;
	*(vu32*)0x800030C4 = 0;
	*(vu32*)0x800030C8 = 0;
	*(vu32*)0x800030D8 = 0;	
	*(vu32*)0x8000315C = 0x81;

//	__asm("lis 3, 0x8000\n");

	*(vu16*)0xCC00501A = 156;

	//*(vu32*)0xCC006400 = 0x340;

	*(vu32*)0x800030CC = 0;
	*(vu32*)0x800030C8 = 0;
	*(vu32*)0x800030D0 = 0;

//	memset( (void*)0x80003040, 0, 0x80 );

	*(vu32*)0x800030C4 = 0;
	*(vu32*)0x800030C8 = 0;

	//*(vu32*)0xCC003004 = 0xF0;
	//*(vu32*)0xCD000034 = 0x4000;

	*(vu32*)0x800030D8 = 0;
	
	*(vu32*)0x8000315C = 0x81;
}
void Initialise()
{
	int i;
	AUDIO_Init(NULL);
	DSP_Init();
	AUDIO_StopDMA();
	AUDIO_RegisterDMACallback(NULL);
	CheckForGecko();
	gprintf("GRRLIB_Init = %i\r\n", GRRLIB_Init());
	myFont = GRRLIB_LoadTTF(font_ttf, font_ttf_size);
	background = GRRLIB_LoadTexturePNG(background_png);
	screen_buffer = GRRLIB_CreateEmptyTexture(rmode->fbWidth, rmode->efbHeight);
	for (i=0; i<255; i +=5) // Fade background image in from black screen
	{
		GRRLIB_DrawImg(0, 0, background, 0, 1, 1, RGBA(255, 255, 255, i)); // Opacity increases as i does
		GRRLIB_Render();
	}
	ClearScreen();
	gprintf("Initialize Finished\r\n");
}
static void (*stub)() = (void*)0x80001800;
inline void DrawBuffer(void) {
	GRRLIB_DrawImg(0, 0, screen_buffer, 0, 1, 1, 0xFFFFFFFF);
}
inline void UpdateScreen(void) {
	GRRLIB_Screen2Texture(0, 0, screen_buffer, GX_FALSE);
	GRRLIB_Render();
	DrawBuffer();
}
void ExitToLoader(int ret)
{
	UpdateScreen();
	UpdateScreen(); // Triple render to ensure it gets seen
	GRRLIB_Render();
	gprintf("Exiting Nintendont...\r\n");
	sleep(3);
	GRRLIB_FreeTexture(background);
	GRRLIB_FreeTexture(screen_buffer);
	GRRLIB_FreeTTF(myFont);
	GRRLIB_Exit();
	CloseDevices();
	memset( (void*)0x92f00000, 0, 0x100000 );
	DCFlushRange( (void*)0x92f00000, 0x100000 );
	if(*(vu32*)0x80001804 == 0x53545542 && *(vu32*)0x80001808 == 0x48415858) //stubhaxx
	{
		VIDEO_SetBlack(TRUE);
		VIDEO_Flush();
		VIDEO_WaitVSync();
		__lwp_thread_stopmultitasking(stub);
	}
	SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
	exit(ret);
}
bool LoadNinCFG()
{
	bool ConfigLoaded = true;
	FILE *cfg = fopen("/nincfg.bin", "rb+");
	if (cfg == NULL)
		return false;

	size_t BytesRead;
	BytesRead = fread(ncfg, 1, sizeof(NIN_CFG), cfg);
	switch( ncfg->Version )
	{
		default:
		{
			ConfigLoaded = false;
			break;
		}
		case 2:
		{
			if(BytesRead != 540)
				ConfigLoaded = false;
		} break;
		case NIN_CFG_VERSION:
		{
			if(BytesRead != sizeof(NIN_CFG))
				ConfigLoaded = false;
		} break;
	}

	if (ncfg->Magicbytes != 0x01070CF6)
		ConfigLoaded = false;

	if (ncfg->Version < 3)	//need to upgrade config from ver 2 to ver 3
	{
		ncfg->MemCardBlocks = 0x2;//251 blocks
		ncfg->Version = 3;
	}
	if (ncfg->Version != NIN_CFG_VERSION)
		ConfigLoaded = false;

	if (ncfg->MaxPads > NIN_CFG_MAXPAD)
		ConfigLoaded = false;

	if (ncfg->MaxPads < 0)
		ConfigLoaded = false;

	fclose(cfg);

	return ConfigLoaded;
}
inline void ClearScreen()
{
	GRRLIB_DrawImg(0, 0, background, 0, 1, 1, 0xFFFFFFFF);
}
extern bool sdio_Deinitialize();
extern void USBStorage_Deinitialize();
void CloseDevices()
{
	closeLog();
	fatUnmount("sd");
	sdio_Deinitialize();
	fatUnmount("usb");
	USBStorage_Deinitialize();
	USB_Deinitialize();
}
static char ascii(char s)
{
  if(s < 0x20) return '.';
  if(s > 0x7E) return '.';
  return s;
}
void hexdump(void *d, int len)
{
	if( d == (void*)NULL )
		return;

	u8 *data;
	int i, off;
	data = (u8*)d;

	for (off=0; off<len; off += 16)
	{
		gprintf("%08x  ",off);

		for(i=0; i<16; i++)
			if((i+off)>=len)
				gprintf("   ");
			else
				gprintf("%02x ",data[off+i]);

		gprintf(" ");

		for(i=0; i<16; i++)
			if((i+off)>=len)
				gprintf(" ");
			else
				gprintf("%c",ascii(data[off+i]));

		gprintf("\r\n");
	}
}
bool IsGCGame(u8 *Buffer)
{
	u32 AMB1 = *(vu32*)(Buffer+0x4);
	u32 GCMagic = *(vu32*)(Buffer+0x1C);
	return (AMB1 == 0x414D4231 || GCMagic == 0xC2339F3D);
}

u32 OffsetCheck[4] = {
	0x210320, //GP1
	0x25C0AC, //GP2
	0x1821C4, //AX
	0x20D7E8, //VS4
};

bool IsTRIGame(char *Path)
{
	u32 DOLOffset = 0;
	char FullPath[300];
	sprintf(FullPath, "%s:%s", GetRootDevice(), Path);
	FILE *f = fopen(FullPath, "rb");
	if(f != NULL)
	{
		fseek(f, 0x420, SEEK_SET);
		fread(&DOLOffset, 1, 4, f);
	}
	else
	{
		char FSTPath[300];
		sprintf(FSTPath, "%ssys/main.dol", FullPath);
		f = fopen(FSTPath, "rb");
	}
	if(f != NULL)
	{
		u32 i;
		u32 BufAtOffset;
		for(i = 0; i < 4; ++i)
		{
			fseek(f, DOLOffset+OffsetCheck[i], SEEK_SET);
			fread(&BufAtOffset, 1, 4, f);
			if(BufAtOffset == 0x386000A8)
			{
				fclose(f);
				gprintf("Found TRIGame %u\n", i);
				return true;
			}
		}
		fclose(f);
	}
	return false;
}
