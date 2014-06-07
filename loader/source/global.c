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
#include "global.h"
#include "exi.h"
#include "Config.h"
#include <gccore.h>
#include <ogc/ipc.h>
#include <ogc/es.h>
#include <ogc/video.h>
#include <ogc/gx_struct.h>
#include <ogc/lwp_threads.h>
#include <ogc/consol.h>
#include <ogc/system.h>
#include <ogc/audio.h>
#include <ogc/dsp.h>
#include <ogc/wiilaunch.h>
#include <stdio.h>
#include <unistd.h>
static GXRModeObj *rmode = NULL;
static void *framebuffer = NULL;
u32 HollywoodRevision;

u32 Region;
u32 POffset;

NIN_CFG* ncfg = (NIN_CFG*)0x93002900;
FILE *cfg;
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
	memset( (void*)0x80003F00, 0, 0x11FC100 );
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
void *Initialise()
{
	AUDIO_Init (NULL);
	DSP_Init ();
	AUDIO_StopDMA();
	AUDIO_RegisterDMACallback(NULL);

	VIDEO_Init();
	
	CheckForGecko();

	rmode = VIDEO_GetPreferredMode(NULL);
	
#ifdef DEBUG
	if( !IsWiiU() )
	{
		if( rmode == &TVNtsc480Int )
			gprintf("VI:TVNtsc480Int\r\n");
		else if( rmode == &TVNtsc480IntDf )
			gprintf("VI:TVNtsc480IntDf\r\n");
		else if( rmode == &TVNtsc480IntAa )
			gprintf("VI:TVNtsc480IntAa\r\n");
		else if( rmode == &TVNtsc480Prog )
			gprintf("VI:TVNtsc480Prog\r\n");
		else if( rmode == &TVNtsc480ProgSoft )
			gprintf("VI:TVNtsc480ProgSoft\r\n");
		else if( rmode == &TVNtsc480ProgAa )
			gprintf("VI:TVNtsc480ProgAa\r\n");

		else if( rmode == &TVPal524IntAa )
			gprintf("VI:TVPal524IntAa\r\n");
		else if( rmode == &TVPal528Int )
			gprintf("VI:TVPal528Int\r\n");
		else if( rmode == &TVPal528IntDf )
			gprintf("VI:TVPal528IntDf\r\n");
		else if( rmode == &TVPal528IntDf )
			gprintf("VI:TVPal528IntDf\r\n");
		else if( rmode == &TVPal528IntDf )
			gprintf("VI:TVPal528IntDf\r\n");
	
		else if( rmode == &TVEurgb60Hz480Int )
			gprintf("VI:TVEurgb60Hz480Int\r\n");
		else if( rmode == &TVEurgb60Hz480IntDf )
			gprintf("VI:TVEurgb60Hz480IntDf\r\n");
		else if( rmode == &TVEurgb60Hz480IntAa )
			gprintf("VI:TVEurgb60Hz480IntAa\r\n");
		else if( rmode == &TVEurgb60Hz480Prog )
			gprintf("VI:TVEurgb60Hz480Prog\r\n");
		else if( rmode == &TVEurgb60Hz480ProgSoft )
			gprintf("VI:TVEurgb60Hz480ProgSoft\r\n");
		else if( rmode == &TVEurgb60Hz480ProgAa )
			gprintf("VI:TVEurgb60Hz480ProgAa\r\n");
	}
#endif

	bool progressive = (CONF_GetProgressiveScan() > 0) && VIDEO_HaveComponentCable();
	switch( *(vu32*)0x800000CC )
	{
		default:
		case 0:
		{
			if(progressive)
				rmode = &TVNtsc480Prog;
			else
				rmode = &TVNtsc480IntDf;
		} break;
		case 1:
		{
			if(progressive)
				rmode = &TVEurgb60Hz480Prog;
			else
				rmode = &TVPal528IntDf;
		} break;
		case 2:
		{
			rmode = &TVMpal480IntDf;
		} break;
		case 5:
		{
			if(progressive)
				rmode = &TVEurgb60Hz480Prog;
			else
				rmode = &TVEurgb60Hz480IntDf;
		} 
	}

	framebuffer = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
//	console_init( framebuffer, 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth*VI_DISPLAY_PIX_SZ );
	
	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(framebuffer);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();

	if(rmode->viTVMode&VI_NON_INTERLACE)
		VIDEO_WaitVSync();
	
//	gprintf("X:%u Y:%u M:%u\r\n", rmode->viWidth, rmode->viHeight, rmode->viTVMode );

	while( VIDEO_GetCurrentFramebuffer() == NULL )
		VIDEO_WaitVSync();

	 unsigned long* fb = (unsigned long*)VIDEO_GetCurrentFramebuffer();
	
	int x,y;

	for( x=0; x < rmode->viWidth / 2; ++x)
	for( y=0; y<rmode->viHeight; ++y)
		fb[ x + y*(rmode->viWidth / 2) ] = 0x00800080;
		
	return framebuffer;
}
static void (*stub)() = (void*)0x80001800;
void ExitToLoader(int ret)
{
	sleep(3);
	ClearScreen();
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
void ClearScreen()
{
	VIDEO_ClearFrameBuffer(rmode, framebuffer, COLOR_BLACK);
	VIDEO_Flush();
	VIDEO_WaitVSync();
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
