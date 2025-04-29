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

// strcasecmp()
#include <strings.h>

#include "global.h"
#include "DI.h"
#include "RealDI.h"
#include "wdvd.h"
#include "string.h"
#include "common.h"
#include "alloc.h"
#include "dol.h"
#include "Config.h"
#include "Patch.h"
#include "Stream.h"
#include "ReadSpeed.h"
#include "ISO.h"
#include "FST.h"
#include "HID.h"
#include "BT.h"
#include "usbstorage.h"

#include "ff_utf8.h"
static u8 DummyBuffer[0x1000] __attribute__((aligned(32)));
extern u32 s_cnt;

#ifndef DEBUG_DI
#define dbgprintf(...)
#else
extern int dbgprintf( const char *fmt, ...);
#endif

struct ipcmessage DI_CallbackMsg;
u32 DI_MessageQueue = 0xFFFFFFFF;
static u8 *DI_MessageHeap = NULL;
bool DI_IRQ = false;
u32 DI_Thread = 0;
s32 DI_Handle = -1;
u64 ISOShift64 = 0;
static u32 Streaming = 0; //internal
extern u32 StreamSize, StreamStart, StreamCurrent, StreamEndOffset;

u32 DiscChangeIRQ	= 0;

static char GamePath[256] ALIGNED(32);
extern const u8 *DiskDriveInfo;
extern u32 FSTMode;
extern u32 RealDiscCMD;
extern u32 RealDiscError;
extern bool wiiVCInternal;
u32 WaitForRealDisc = 0;
u32 DiscRequested = 0;

u8 *const DI_READ_BUFFER = (u8*)0x12E80000;
const u32 DI_READ_BUFFER_LENGTH = 0x80000;

extern u32 GAME_ID;
extern u16 GAME_ID6;
extern u32 TITLE_ID;
 
// Triforce
extern vu32 TRIGame;
extern vu32 useipltri;

static u32 GCAMKeyA;
static u32 GCAMKeyB;
static u32 GCAMKeyC;

static u8 *MediaBuffer;
static u8 *NetworkCMDBuffer;
static u8 *const DIMMMemory = (u8*)0x12B80000;

// Multi-disc filenames.
static const char disc_filenames[8][16] = {
	// Disc 1
	"game.ciso", "game.cso", "game.gcm", "game.iso",
	// Disc 2
	"disc2.ciso", "disc2.cso", "disc2.gcm", "disc2.iso"
};

// Filename portions for 2-disc mode.
// Points to entries in disc_filenames.
// NOTE: If either is NULL, assume single-disc mode.
static const char *DI_2disc_filenames[2] = {NULL, NULL};

void DIRegister(void)
{
	DI_MessageHeap = malloca(0x20, 0x20);
	DI_MessageQueue = mqueue_create( DI_MessageHeap, 8 );
	device_register( "/dev/mydi", DI_MessageQueue );
}

void DIUnregister(void)
{
	mqueue_destroy(DI_MessageQueue);
	DI_MessageQueue = 0xFFFFFFFF;
	free(DI_MessageHeap);
	DI_MessageHeap = NULL;
}

bool DiscCheckAsync( void )
{
	if(RealDiscCMD)
		RealDI_Update();
	return (DI_CallbackMsg.result == 0);
}

static void DiscReadAsync(u32 Buffer, u32 Offset, u32 Length, u32 Mode)
{
	DIFinishAsync(); //if something is still running
	DI_CallbackMsg.result = -1;
	sync_after_write(&DI_CallbackMsg, 0x20);
	IOS_IoctlAsync( DI_Handle, Mode, (void*)Offset, 0, (void*)Buffer, Length, DI_MessageQueue, &DI_CallbackMsg );
}

void DiscReadSync(u32 Buffer, u32 Offset, u32 Length, u32 Mode)
{
	DiscReadAsync(Buffer, Offset, Length, Mode);
	DIFinishAsync();
}

//ISO Cache is disabled while SegaBoot runs
static u8 *const SegaBoot = (u8*)0x12A80000;
void ReadSegaBoot(u32 Buffer, u32 Offset, u32 Length)
{
	if(Offset > 0x100000) //set invalid
		memset32((void*)Buffer, 0, Length);
	else //part of actual ipl
	{
		sync_before_read(SegaBoot+Offset, Length);
		memcpy((void*)Buffer, SegaBoot+Offset, Length);
		DoPatches((void*)Buffer, Length, Offset);
	}
	sync_after_write((void*)Buffer, Length);
}

void DIinit( bool FirstTime )
{
	//This debug statement seems to cause crashing.
	//dbgprintf("DIInit()\r\n");

	if(DI_Handle >= 0) //closes old file
		IOS_Close(DI_Handle);

	if (FirstTime)
	{
		if(wiiVCInternal)
		{
			if(WDVD_Init() != 0)
				Shutdown();
			if(!WDVD_FST_Mount())
				Shutdown();
		}
		else if(RealDiscCMD == 0)
		{
			// Check if this is a 2-disc game.
			u32 i, slash_pos;
			char TempDiscName[256];
			strcpy(TempDiscName, ConfigGetGamePath());

			//search the string backwards for '/'
			for (slash_pos = strlen(TempDiscName); slash_pos > 0; --slash_pos)
			{
				if (TempDiscName[slash_pos] == '/')
					break;
			}
			slash_pos++;

			// First, check if the disc filename matches
			// the expected filenames for multi-disc games.
			int checkIdxMin = -1, checkIdxMax = -1;
			const char **DI_2disc_otherdisc = NULL;
			DI_2disc_filenames[0] = NULL;
			DI_2disc_filenames[1] = NULL;
			for (i = 0; i < 8; i++)
			{
				if (!strcasecmp(TempDiscName+slash_pos, disc_filenames[i]))
				{
					// Filename is either:
					// -  game.(ciso|cso|gcm|iso) (Disc 1)
					// - disc2.(ciso|cso|gcm|iso) (Disc 2)
					const int discIdx = i / 4;	// either 0 or 1
					DI_2disc_filenames[discIdx] = disc_filenames[i];

					// Set variables to check for the other disc.
					if (discIdx == 0) {
						checkIdxMin = 4;
						checkIdxMax = 7;
						DI_2disc_otherdisc = &DI_2disc_filenames[1];
					} else {
						checkIdxMin = 0;
						checkIdxMax = 3;
						DI_2disc_otherdisc = &DI_2disc_filenames[0];
					}
					break;
				}
			}

			if (DI_2disc_otherdisc != NULL)
			{
				// Check for the other disc.
				for (i = checkIdxMin; i <= checkIdxMax; i++)
				{
					strcpy(TempDiscName+slash_pos, disc_filenames[i]);
					FIL ExistsFile;
					s32 ret = f_open_char(&ExistsFile, TempDiscName, FA_READ);
					if (ret == FR_OK)
					{
						// Found the other disc image.
						f_close(&ExistsFile);
						*DI_2disc_otherdisc = disc_filenames[i];
						break;
					}
				}
			}
		}
		write32( DIP_STATUS, 0x54 ); //mask and clear interrupts
		write32( DIP_COVER, 4 ); //disable cover irq which DIP enabled

		sync_before_read((void*)0x13003000, 0x20);
		ISOShift64 = (u64)(read32(0x1300300C)) << 2;

		MediaBuffer = (u8*)malloc( 0x40 );
		memset32( MediaBuffer, 0, 0x40 );

		NetworkCMDBuffer = (u8*)malloc( 512 );
		memset32( NetworkCMDBuffer, 0, 512 );

		//This normally contains default rankings but we just clear it
		memset32( DIMMMemory, 0, 0x300000 );

		memset32( (void*)DI_BASE, 0, 0x30 );
		sync_after_write( (void*)DI_BASE, 0x40 );
	}
	DI_Handle = IOS_Open( "/dev/mydi", 0 );

	GAME_ID = read32(0);
	GAME_ID6 = R16(4);
	TITLE_ID = (GAME_ID >> 8);

	GCAMKeyA = read32(0);
	GCAMKeyB = read32(4);
	GCAMKeyC = read32(8);

	ReadSpeed_Init();
}
void DISetDIMMVersion( u32 Version )
{
	write32((u32)DIMMMemory, Version);
}
bool DIChangeDisc( u32 DiscNumber )
{
	if(wiiVCInternal)
	{
		if(DiscNumber > 1)
			return false;
		DiscRequested = DiscNumber;
	}
	else
	{
		// Don't do anything if multi-disc mode isn't enabled.
		if (!DI_2disc_filenames[0] ||
			!DI_2disc_filenames[1] ||
			DiscNumber > 1)
		{
			return false;
		}

		u32 slash_pos;
		char* DiscName = ConfigGetGamePath();

		//search the string backwards for '/'
		for (slash_pos = strlen(DiscName); slash_pos > 0; --slash_pos)
		{
			if (DiscName[slash_pos] == '/')
				break;
		}
		slash_pos++;

		_sprintf(DiscName+slash_pos, DI_2disc_filenames[DiscNumber]);
		dbgprintf("New Gamepath:\"%s\"\r\n", DiscName );
	}
	DIinit(false);
	return true;
}

void DIInterrupt()
{
	if(ReadSpeed_End() == 0)
		return; //still busy
	sync_before_read( (void*)DI_BASE, 0x40 );
	/* Update DMA registers when needed */
	if(read32(DI_CONTROL) & 2)
	{
		if( TITLE_ID == 0x47544B || //Turok Evolution
			TITLE_ID == 0x47514C || //Dora the Explorer
			TRIGame != TRI_NONE )
		{	/* Manually invalidate data */
			write32(DI_INV_ADR, read32(DI_DMA_ADR));
			write32(DI_INV_LEN, read32(DI_DMA_LEN));
		}
		write32(DI_DMA_ADR, read32(DI_DMA_ADR) + read32(DI_DMA_LEN));
	}
	write32( DI_CONTROL, read32(DI_CONTROL) & 2 ); // finished command
	u32 di_status = read32(DI_STATUS);
	if(RealDiscError == 0)
	{
		write32( DI_DMA_LEN, 0 ); // all data handled, clear length
		write32( DI_STATUS, di_status | 0x10 ); //set TC
		sync_after_write( (void*)DI_BASE, 0x40 );
		if( di_status & 0x8 ) //TC Interrupt enabled
		{
			write32( DI_INT, 0x4 ); // DI IRQ
			sync_after_write( (void*)DI_INT, 0x20 );
			write32( HW_IPC_ARMCTRL, 8 ); //throw irq
			//dbgprintf("Disc Interrupt\r\n");
		}
	}
	else
	{
		write32( DI_STATUS, di_status | 4 ); //set Error
		sync_after_write( (void*)DI_BASE, 0x40 );
		if( di_status & 2 ) //Error Interrupt enabled
		{
			write32( DI_INT, 0x4 ); // DI IRQ
			sync_after_write( (void*)DI_INT, 0x20 );
			write32( HW_IPC_ARMCTRL, 8 ); //throw irq
			//dbgprintf("Disc Interrupt\r\n");
		}
	}
	DI_IRQ = false;
}

static void TRIReadMediaBoard( u32 Buffer, u32 Offset, u32 Length )
{
	sync_before_read( (void*)Buffer, Length );
	if( (Offset & 0x8FFF0000) == 0x80000000 )
	{
		switch(Offset)
		{
			// Media board status (1)
			case 0x80000000:
				W16( Buffer, 0x0100 );
				break;
			// Media board status (2)
			case 0x80000020:
				memset( (void*)Buffer, 0, Length );
				break;
			// Media board status (3)
			case 0x80000040:
				memset( (void*)Buffer, 0xFF, Length );
				// DIMM size (512MB)
				W32( Buffer, 0x00000020 );
				// GCAM signature
				W32( Buffer+4, 0x4743414D );
				break;
			// Firmware status (1)
			case 0x80000120:
				W32( Buffer, 0x00000001 );
				break;
			// Firmware status (2)
			case 0x80000140:
				W32( Buffer, 0x01000000 );
				break;
			default:
				dbgprintf("Unhandled Media Board Read:%08X\n", Offset );
				break;
		}
	}  // DIMM memory (3MB)
	else if( (Offset >= 0x1F000000) && (Offset <= 0x1F300000) )
	{
		u32 roffset = Offset - 0x1F000000;
		memcpy( (void*)Buffer, DIMMMemory + roffset, Length );
	}  // DIMM command
	else if( (Offset >= 0x1F900000) && (Offset <= 0x1F900040) )
	{
		u32 roffset = Offset - 0x1F900000;
	#ifdef DEBUG_GCAM
		dbgprintf("GC-AM: Read MEDIA BOARD COMM AREA (%08x)\n", roffset );
	#endif
		memcpy( (void*)Buffer, MediaBuffer + roffset, Length );
	}  // Network command
	else if( (Offset >= 0x1F800200) && (Offset <= 0x1F8003FF) )
	{
		u32 roffset = Offset - 0x1F800200;
	#ifdef DEBUG_GCAM
		dbgprintf("GC-AM: Read MEDIA BOARD NETWORK COMMAND (%08x)\n", roffset );
	#endif
		memcpy( (void*)Buffer, NetworkCMDBuffer + roffset, Length );
	}
	sync_after_write( (void*)Buffer, Length );
}

void DIUpdateRegisters( void )
{
	if( DI_IRQ == true ) //still working
		return;

	u32 i;
	u32 DIOK = 0,DIcommand;

	sync_before_read( (void*)DI_BASE, 0x40 );
	if( read32(DI_CONTROL) & 1 )
	{
		udelay(50); //security delay
		write32( DI_STATUS, read32(DI_STATUS) & 0x2A ); //clear status
#ifndef TRI_DI_PATCH
		/*
			Trifroce is sending all DI commands encrypted since by chance a crypted command
			could have the same value as a normal command we have to check if it really is
			a normal command or not.
		*/
		if(TRIGame)
		{
			switch( read32(DI_CMD_0) >> 8 )
			{
				case 0x120000:
				case 0xA70000:
				case 0xA80000:
				case 0xAB0000:
				case 0xE00000:
				case 0xE10000: case 0xE10100:
				case 0xE20000: case 0xE20100: case 0xE20200: case 0xE20300:
				case 0xE30000:
				case 0xE40000:
				case 0xE40100:
				case 0xF80000:
				case 0xF90000:
					DIcommand = read32(DI_CMD_0) >> 24;
				break;
				default:
					//Decrypt command
					write32( DI_CMD_0, read32(DI_CMD_0) ^ GCAMKeyA );
					write32( DI_CMD_1, read32(DI_CMD_1) ^ GCAMKeyB );
					write32( DI_CMD_2, read32(DI_CMD_2) ^ GCAMKeyC );

					//Update keys
					u32 seed = read32(DI_CMD_0) >> 16;

					GCAMKeyA = GCAMKeyA * seed;
					GCAMKeyB = GCAMKeyB * seed;
					GCAMKeyC = GCAMKeyC * seed;

					DIcommand = read32(DI_CMD_0) & 0xFF;
					break;
			}
		} else {
			DIcommand = read32(DI_CMD_0) >> 24;
		}
#else
		//no encryption here, just direct CMD
		DIcommand = read32(DI_CMD_0) >> 24;
#endif
		switch( DIcommand )
		{
			default:
			{
				dbgprintf("DI: Unknown command:%02X\r\n", DIcommand );

				for( i = 0; i < 0x30; i+=4 )
					dbgprintf("0x%08X:0x%08X\r\n", i, read32( DI_BASE + i ) );
				dbgprintf("\r\n");

				memset32( (void*)DI_BASE, 0xdeadbeef, 0x30 );
				Shutdown();

			} break;
			case 0xE1:	// play Audio Stream
			{
				//dbgprintf("DIP:DVDAudioStream(%d)\n", (read32(DI_CMD_0) >> 16 ) & 0xFF );
				switch( (read32(DI_CMD_0) >> 16) & 0xFF )
				{
					case 0x00:
						StreamStartStream(read32(DI_CMD_1) << 2, read32(DI_CMD_2));
						Streaming = 1;
						break;
					case 0x01:
						StreamEndStream();
						Streaming = 0;
						break;
					default:
						break;
				}
				DIOK = 2;
			} break;
			case 0xE2:	// request Audio Status
			{
				switch( (read32(DI_CMD_0) >> 16) & 0xFF )
				{
					case 0x00:	// Streaming?
						Streaming = !!(StreamCurrent);
						write32( DI_IMM, Streaming );
						break;
					case 0x01:	// What is the current address?
						if(Streaming)
						{
							if(StreamCurrent)
								write32( DI_IMM, ALIGN_BACKWARD(StreamCurrent, 0x8000) >> 2 );
							else
								write32( DI_IMM, StreamEndOffset >> 2 );
						}
						else
							write32( DI_IMM, 0 );
						break;
					case 0x02:	// disc offset of file
						write32( DI_IMM, StreamStart >> 2 );
						break;
					case 0x03:	// Size of file
						write32( DI_IMM, StreamSize );
						break;
					default:
						break;
				}
				//dbgprintf("DIP:DVDAudioStatus( %d, %08X )\n", (read32(DI_CMD_0) >> 16) & 0xFF, read32(DI_IMM) );
				DIOK = 2;
			} break;
			case 0x12:
			{
				if(TRIGame)
				{
					#ifdef DEBUG_GCAM
					dbgprintf("GC-AM: 0x12\n");
					#endif
					//if(TRIGame == TRI_GP2) //this is what it SHOULD be
					//	write32( DI_IMM, 0x29000000 );
					//else
						write32( DI_IMM, 0x21000000 );
				}
				else if(read32(DI_CMD_2) == 32)
				{
					//dbgprintf("DIP:DVDLowInquiry( 0x%08x, 0x%08x )\r\n", read32(DI_DMA_ADR), read32(DI_DMA_LEN));
					void *Buffer = (void*)P2C(read32(DI_DMA_ADR));
					memcpy( Buffer, DiskDriveInfo, 32 );
					sync_after_write( Buffer, 32 );
				}
				DIOK = 2;
			} break;
			case 0xAA:
			{
				u32 Buffer	= P2C(read32(DI_DMA_ADR));
				u32 Length	= read32(DI_CMD_2);
				u32 Offset	= read32(DI_CMD_1) << 2;
				//dbgprintf( "GCAM:Write( 0x%08x, 0x%08x, 0x%08x )\n", Offset, Length, Buffer|0x80000000 );

				sync_before_read( (void*)Buffer, Length );

				// DIMM memory (3MB)
				if( (Offset >= 0x1F000000) && (Offset <= 0x1F300000) )
				{
					u32 roffset = Offset - 0x1F000000;
					memcpy( DIMMMemory + roffset, (void*)Buffer, Length );
				}

				// DIMM command
				if( (Offset >= 0x1F900000) && (Offset <= 0x1F900040) )
				{
					u32 roffset = Offset - 0x1F900000;
					#ifdef DEBUG_GCAM
					dbgprintf("GC-AM: Write MEDIA BOARD COMM AREA (%08x)\n", roffset );
					#endif
					memcpy( MediaBuffer + roffset, (void*)Buffer, Length );
				}

				// Network command
				if( (Offset >= 0x1F800200) && (Offset <= 0x1F8003FF) )
				{
					u32 roffset = Offset - 0x1F800200;
					#ifdef DEBUG_GCAM
					dbgprintf("GC-AM: Write MEDIA BOARD NETWORK COMMAND (%08x)\n", roffset );
					#endif
					memcpy( NetworkCMDBuffer + roffset, (void*)Buffer, Length );
				}
				DIOK = 2;
			} break;
			case 0xAB:
			{
				if(TRIGame)
				{
					memset32( MediaBuffer, 0, 0x20 );

					MediaBuffer[0] = MediaBuffer[0x20];

					// Command
					*(u16*)(MediaBuffer+2) = *(u16*)(MediaBuffer+0x22) | 0x80;

					u16 cmd = bswap16(*(u16*)(MediaBuffer+0x22));

					#ifdef DEBUG_GCAM
					if(cmd)
						dbgprintf("GCAM:Execute command:%03X\n", cmd );
					#endif
					switch( cmd )
					{
						// ?
						case 0x000:
							*(u32*)(MediaBuffer+4) = bswap32(1);
							break;
						// DIMM size
						case 0x001:
							*(u32*)(MediaBuffer+4) = bswap32(0x20000000);
							break;
						// Media board status
						case 0x100:
							// Status
							*(u32*)(MediaBuffer+4) = bswap32(5);
							// Progress in %
							*(u32*)(MediaBuffer+8) = bswap32(100);
						break;
						// Media board version: 13.05
						case 0x101:
							// Version
							*(u16*)(MediaBuffer+4) = bswap16(0x1305);
							// Unknown
							*(u16*)(MediaBuffer+6) = 0x01;
							*(u32*)(MediaBuffer+8) = 1;
							*(u32*)(MediaBuffer+16)= 0xFFFFFFFF;
						break;
						// System flags
						case 0x102:
							// 1: GD-ROM
							MediaBuffer[4] = 1;
							MediaBuffer[5] = 0;
							// enable development mode (Sega Boot)
							MediaBuffer[6] = 1;
						break;
						// Media board serial
						case 0x103:
							memcpy( MediaBuffer + 4, "A89E28A48984511", 16 );
						break;
						case 0x104:
							MediaBuffer[4] = 1;
						break;
						// Get IP (?)
						case 0x406:
						{
							char *IP			= (char*)(NetworkCMDBuffer + ( bswap32(*(u32*)(MediaBuffer+0x28)) - 0x1F800200 ) );
							u32 IPLength	= bswap32(*(u32*)(MediaBuffer+0x2C));

							memcpy( MediaBuffer + 4, IP, IPLength );

							#ifdef DEBUG_GCAM
							dbgprintf( "GC-AM: Get IP:%s\n", (char*)(MediaBuffer + 4) );
							#endif
						} break;
						// Set IP
						case 0x415:
						{
							#ifdef DEBUG_GCAM
							char *IP			= (char*)(NetworkCMDBuffer + ( bswap32(*(u32*)(MediaBuffer+0x28)) - 0x1F800200 ) );
							u32 IPLength	= bswap32(*(u32*)(MediaBuffer+0x2C));
							dbgprintf( "GC-AM: Set IP:%s, Length:%d\n", IP, IPLength );
							#endif
						} break;
						default:
						{
							//dbgprintf("GC-AM: Unknown DIMM Command\n");
							//hexdump( MediaBuffer + 0x20, 0x20 );
						} break;
					}
					memset32( MediaBuffer + 0x20, 0, 0x20 );
					write32( DI_IMM, 0x66556677 );
				}
				else if(FSTMode == 0 && RealDiscCMD == 0)
				{
					u32 Offset = read32(DI_CMD_1) << 2;
					//dbgprintf("DIP:DVDLowSeek( 0x%08X )\r\n", Offset);
					ISOSeek(Offset);
				}
				DIOK = 2;
			} break;
			case 0xE0:	// Get error status
			{
				if(WaitForRealDisc == 0 && RealDiscError == 0)
					write32( DI_IMM, 0x00000000 );
				else
				{	//we just always say disc got removed as error
					write32(DI_IMM, 0x1023a00);
					write32(DI_COVER, 1);
					RealDiscError = 0;
					WaitForRealDisc = 1;
				}
				DIOK = 2;
			} break;
			case 0xE3:	// stop Motor
			{
				dbgprintf("DIP:DVDLowStopMotor()\r\n");
				if(RealDiscCMD == 0)
				{
					u32 CDiscNumber = (read32(4) << 16 ) >> 24;
					dbgprintf("DIP:Current disc number:%u\r\n", CDiscNumber + 1 );

					if (DIChangeDisc( CDiscNumber ^ 1 ))
						DiscChangeIRQ = 1;
				}
				else if (!Datel)
				{	//we just always say disc got removed as error
					write32(DI_IMM, 0x1023a00);
					write32(DI_COVER, 1);
					RealDiscError = 0;
					WaitForRealDisc = 1;
				}
				ReadSpeed_Motor();
				DIOK = 2;

			} break;
			case 0xE4:	// Disable Audio
			{
				dbgprintf("DIP:DVDDisableAudio()\r\n");
				DIOK = 2;
			} break;
			case 0xA8:
			{
				u32 rawAddr = read32(DI_DMA_ADR);
				u32 Buffer	= P2C(rawAddr);
				u32 Length	= read32(DI_CMD_2);
				u32 Offset	= read32(DI_CMD_1) << 2;

				/* Part of SegaBoot Init */
				if(TRIGame == TRI_SB && Offset == 0x420)
				{
					//dbgprintf("DIP:Switching to actual Triforce game\r\n");
					useipltri = 0; //happens after the "setup", read actual disc
				}

				dbgprintf( "DIP:DVDReadA8( 0x%08x, 0x%08x, 0x%08x )\r\n", Offset, Length, rawAddr );

				if( TRIGame && Offset >= 0x1F000000 )
				{
					TRIReadMediaBoard( Buffer, Offset, Length );
					DIOK = 2;
				}
				else
				{
					if( Buffer < 0x01800000 || // Valid GC Buffer
						rawAddr == 0x931C1800 ) //PSO Ep.III Hack
					{
						if( useipltri )
							ReadSegaBoot(Buffer, Offset, Length);
						else
						{
							DiscReadAsync(Buffer, Offset, Length, 0);
							ReadSpeed_Start();
						}
					}
					DIOK = 2;
				}
			} break;
			case 0xF8:
			{
				u32 Buffer	= P2C(read32(DI_DMA_ADR));
				u32 Length	= read32(DI_CMD_2);
				u32 Offset	= read32(DI_CMD_1) << 2;

				dbgprintf( "DIP:DVDReadF8( 0x%08x, 0x%08x, 0x%08x )\r\n", Offset, Length, Buffer|0x80000000 );

				if( Buffer < 0x01800000 )
				{
					if( useipltri )
						ReadSegaBoot(Buffer, Offset, Length);
					else
						DiscReadSync(Buffer, Offset, Length, 0);
				}
				DIOK = 1;
			} break;
			case 0xF9:
			{
				#ifdef PATCHALL
				BTInit();
				#endif
				DIOK = 1;
			} break;
		}

		if( DIOK )
		{
			if( DIOK == 2 )
				DI_IRQ = true;
			else
				write32(DI_CONTROL, 0);
		}
		sync_after_write( (void*)DI_BASE, 0x40 );
	}
	return;
}

extern u32 Patch31A0Backup;
static const u8 *di_src = NULL;
static char *di_dest = NULL;
static u32 di_length = 0;
static u32 di_offset = 0;
u32 DIReadThread(void *arg)
{
	//dbgprintf("DI Thread Running\r\n");
	struct ipcmessage *di_msg = NULL;
	while(1)
	{
		mqueue_recv( DI_MessageQueue, &di_msg, 0 );
		switch( di_msg->command )
		{
			case IOS_OPEN:
				if( strncmp("/dev/mydi", di_msg->open.device, 8 ) != 0 )
				{	//this should never happen
					mqueue_ack( di_msg, -6 );
					break;
				}
				if(RealDiscCMD == 0)
				{
					ISOClose();
					if( ISOInit() == false )
					{
						_sprintf( GamePath, "%s", ConfigGetGamePath() );
						//Try to switch to FST mode
						if( !FSTInit(GamePath) )
						{
							//dbgprintf("Failed to open:%s Error:%u\r\n", ConfigGetGamePath(), ret );	
							Shutdown();
						}
					}
				}
				mqueue_ack( di_msg, 24 );
				break;

			case IOS_CLOSE:
				if(RealDiscCMD == 0)
				{
					if( FSTMode )
						FSTCleanup();
					else
						ISOClose();
				}
				mqueue_ack( di_msg, 0 );
				break;

			case IOS_IOCTL:
				if(di_msg->ioctl.command == 2)
				{
					USBStorage_ReadSectors(read32(HW_TIMER) % s_cnt, 1, DummyBuffer);
					mqueue_ack( di_msg, 0 );
					break;
				}
				di_src = 0;
				di_dest = (char*)di_msg->ioctl.buffer_io;
				di_length = di_msg->ioctl.length_io;
				di_offset = (u32)di_msg->ioctl.buffer_in;
				u32 Offset = 0;
				u32 Length = di_length;
				for (Offset = 0; Offset < di_length; Offset += Length)
				{
					// NOTE: ISOShift64 is applied in the RealDI and ISO readers.
					// Not supported for FST.
					Length = di_length - Offset;
					if( RealDiscCMD )
						di_src = ReadRealDisc(&Length, di_offset + Offset, true);
					else if( FSTMode )
						di_src = FSTRead(GamePath, &Length, di_offset + Offset);
					else
						di_src = ISORead(&Length, di_offset + Offset);
					// Copy data at a later point to prevent MEM1 issues
					if(((u32)di_dest + Offset) <= 0x31A0)
					{
						u32 pos31A0 = 0x31A0 - ((u32)di_dest + Offset);
						Patch31A0Backup = read32((u32)di_src + pos31A0);
					}
					memcpy( di_dest + Offset, di_src, Length > di_length ? di_length : Length );
				}
				if(di_msg->ioctl.command == 0)
				{
					DoPatches(di_dest, di_length, di_offset);
					ReadSpeed_Setup(di_offset, di_length);
				}
				sync_after_write( di_dest, di_length );
				mqueue_ack( di_msg, 0 );
				break;

			case IOS_ASYNC:
				mqueue_ack( di_msg, 0 );
				break;
		}
	}
	return 0;
}

void DIFinishAsync()
{
	while(DiscCheckAsync() == false)
	{
		udelay(200); //wait for driver
		CheckOSReport();
		BTUpdateRegisters();
	}
}
/*
struct _TGCInfo
{
	u32 tgcoffset;
	u32 doloffset;
	u32 fstoffset;
	u32 fstsize;
	u32 userpos;
	u32 fstupdate;
	u32 isTGC;
};
static struct _TGCInfo *const TGCInfo = (struct _TGCInfo*)0x130031E0;

#define PATCH_STATE_PATCH 2
extern u32 PatchState, DOLSize, DOLMinOff, DOLMaxOff;

bool DICheckTGC(u32 Buffer, u32 Length)
{
	if(*(vu32*)di_dest == 0xAE0F38A2) //TGC Magic
	{	//multidol, loader always comes after reading tgc header
		dbgprintf("Found TGC Header\n");
		TGCInfo->tgcoffset = di_offset + *(vu32*)(di_dest + 0x08);
		TGCInfo->doloffset = di_offset + *(vu32*)(di_dest + 0x1C);
		TGCInfo->fstoffset = di_offset + *(vu32*)(di_dest + 0x10);
		TGCInfo->fstsize = *(vu32*)(di_dest + 0x14);
		TGCInfo->userpos = *(vu32*)(di_dest + 0x24);
		TGCInfo->fstupdate = *(vu32*)(di_dest + 0x34) - *(vu32*)(di_dest + 0x24) - di_offset;
		TGCInfo->isTGC = 1;
		sync_after_write((void*)TGCInfo, sizeof(struct _TGCInfo));
		return true;
	}
	else if(di_offset == 0x2440)
	{
		u16 company = (read32(0x4) >> 16);
		if(company == 0x3431 || company == 0x3730 || TITLE_ID == 0x474143 || TITLE_ID == 0x47434C || TITLE_ID == 0x475339)
		{	//we can patch the loader in this case, that works for some reason
			dbgprintf("Game is resetting to original loader, using original\n");
			PatchState = PATCH_STATE_PATCH;
			DOLSize = Length;
			DOLMinOff = Buffer;
			DOLMaxOff = DOLMinOff + Length;
		}
		else
		{	//multidol, just clear tgc data structure for it to load the original
			dbgprintf("Game is resetting to original loader, using multidol\n");
			memset32((void*)TGCInfo, 0, sizeof(struct _TGCInfo));
			sync_after_write((void*)TGCInfo, sizeof(struct _TGCInfo));
			return true;
		}
	}
	else
		dbgprintf("Game is loading another DOL\n");
	return false;
}
*/