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
#include "DI.h"
#include "string.h"
#include "syscalls.h"
#include "global.h"
#include "ipc.h"
#include "common.h"
#include "alloc.h"
#include "StreamADPCM.h"
#include "ff.h"
#include "dol.h"
#include "Config.h"
#include "Patch.h"
#include "FST.h"

#ifndef DEBUG_DI
#define dbgprintf(...)
#else
extern int dbgprintf( const char *fmt, ...);
#endif

struct ipcmessage DI_CallbackMsg;
u32 DI_MessageQueue = 0xFFFFFFFF;
u8 *DI_MessageHeap = NULL;
bool DI_IRQ = false;
u32 DI_Thread = 0;
s32 DI_Handle = -1;

u32	StreamBufferSize= 0x1E00000;
u32 StreamBuffer	= 0x11200000+0x60;
u32 Streaming		= 0;
u32	StreamOffset	= 0;
u32 StreamDiscOffset= 0;
s32 StreamSize		= 0;
u32 StreamRAMOffset	= 0;
u32 StreamTimer		= 0;
u32 StreamStopEnd	= 0;
u32 DiscChangeIRQ	= 0;

FIL GameFile;

static char GamePath[256] ALIGNED(32);
static char *FSTBuf = (char *)(0x13200000);
extern u32 Region;
extern u32 FSTMode;

// Triforce
extern u32 TRIGame;

u32 GCAMKeyA;
u32 GCAMKeyB;
u32 GCAMKeyC;

u8 *MediaBuffer;
u8 *NetworkCMDBuffer;
u8 *DIMMMemory = (u8*)0x11280000;

void DIRegister(void)
{
	DI_MessageHeap = malloca(0x20, 0x20);
	DI_MessageQueue = mqueue_create( DI_MessageHeap, 8 );
	device_register( "/dev/di", DI_MessageQueue );
}

void DIUnregister(void)
{
	mqueue_destroy(DI_MessageQueue);
	DI_MessageQueue = 0xFFFFFFFF;
	free(DI_MessageHeap);
	DI_MessageHeap = NULL;
}

void DIinit( bool FirstTime )
{
	//This debug statement seems to cause crashing.
	//dbgprintf("DIInit()\r\n");

	if(DI_Handle >= 0) //closes old file
		IOS_Close(DI_Handle);
	DI_Handle = IOS_Open( "/dev/di", 0 );

	if (FirstTime)
	{
		GCAMKeyA = read32(0);
		GCAMKeyB = read32(4);
		GCAMKeyC = read32(8);

		MediaBuffer = (u8*)malloc( 0x40 );
		memset32( MediaBuffer, 0, 0x40 );

		NetworkCMDBuffer = (u8*)malloc( 512 );
		memset32( NetworkCMDBuffer, 0, 512 );

		memset32( (void*)DI_BASE, 0xdeadbeef, 0x30 );
		memset32( (void*)DI_SHADOW, 0, 0x30 );

		sync_after_write( (void*)DI_BASE, 0x60 );

		write32( DIP_IMM, 0 ); //reset errors
		write32( DIP_STATUS, 0x2E );
		write32( DIP_CMD_0, 0xE3000000 ); //spam stop motor
	}
	else
	{
		CacheInit(FSTBuf, true);
	}
}
static char discstr[0x100] __attribute__((aligned(0x20)));
void DIChangeDisc( u32 DiscNumber )
{
	u32 read, i;
	char* DiscName = ConfigGetGamePath();

	//search the string backwards for '/'
	for( i=strlen(DiscName); i > 0; --i )
		if( DiscName[i] == '/' )
			break;
	i++;

	if( DiscNumber == 0 )
		_sprintf( DiscName+i, "game.iso" );
	else
		_sprintf( DiscName+i, "disc2.iso" );

	dbgprintf("New Gamepath:\"%s\"\r\n", DiscName );
	DIinit(false); //closes previous file and opens the new one

	memset32(discstr, 0, 0x100);
	f_lseek( &GameFile, 0x0 );
	f_read( &GameFile, (void*)discstr, 0x100, &read ); // Loading the full 0x400 causes problems.
	discstr[0xFF] = '\0';

	dbgprintf("DIP:Loading game %.6s: %s\r\n", discstr, (char *)(discstr+0x20) );

	//f_lseek( &GameFile, 0x420 );
	//f_read( &GameFile, str, 0x40, &read );

	// str was not alloced.  Do not free.
	//free(str);
}
static void DIReadISO()
{
	u32 read;
	/* Set Low Mem */
	f_lseek( &GameFile, 0 );
	f_read( &GameFile, (void*)0, 0x20, &read );
	sync_after_write( (void*)0, 0x20 );
	/* Prepare Cache Files */
	u32 FSTOffset, FSTSize;
	f_lseek( &GameFile, 0x424 );
	f_read( &GameFile, &FSTOffset, sizeof(u32), &read );
	f_lseek( &GameFile, 0x428 );
	f_read( &GameFile, &FSTSize, sizeof(u32), &read );
	f_lseek( &GameFile, FSTOffset );
	f_read( &GameFile, FSTBuf, FSTSize, &read );
	sync_after_write( FSTBuf, FSTSize );
	/* Get Region */
	f_lseek( &GameFile, 0x458 );
	f_read( &GameFile, &Region, sizeof(u32), &read );
}
void DIInterrupt()
{
	DI_IRQ = false;

	clear32(DI_SSTATUS, (1<<2) | (1<<4)); //transfer complete, no errors
	sync_after_write((void*)DI_SSTATUS, 4);

	write32( DIP_CONTROL, 1 ); //start transfer, causes an interrupt, game gets its data

	//some games might need this
	//while( read32(DIP_CONTROL) & 1 ) ;
	//clear32(DI_SCONTROL, (1<<0)); //game shadow controls, just in case its not interrupt based
	//sync_after_write((void*)DI_SCONTROL, 4);
}
void DIUpdateRegisters( void )
{
	if(DI_IRQ == true)
		return;

	u32 i;
	u32 DIOK = 0,DIcommand;

	u32 *DInterface	 = (u32*)(DI_BASE);
	u32 *DInterfaceS = (u32*)(DI_SHADOW);

	sync_before_read( (void*)DI_BASE, 0x60 );

	if( read32(DI_CONTROL) != 0xdeadbeef )
	{
		write32( DI_SCONTROL, read32(DI_CONTROL) & 3 );

		//*(vu32*)DI_SSTATUS &= ~0x14;

		write32( DI_CONTROL, 0xdeadbeef );
		sync_after_write( (void*)DI_BASE, 0x60 );

		if( read32(DI_SCONTROL) & 1 )
		{
			for( i=2; i < 9; ++i )
			{
				if( DInterface[i] != 0xdeadbeef )
				{
					DInterfaceS[i] = DInterface[i];
					DInterface[i]  = 0xdeadbeef;
				}
			}

			/*
				Trifroce is sending all DI commands encrypted since by chance a crypted command
				could have the same value as a normal command we have to check if it really is
				a normal command or not.
			*/
			if(TRIGame)
			{
				switch( read32(DI_SCMD_0) >> 8 )
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
					case 0xF90000:
						DIcommand = read32(DI_SCMD_0) >> 24;
					break;
					default:
						//Decrypt command
						write32( DI_SCMD_0, read32(DI_SCMD_0) ^ GCAMKeyA );
						write32( DI_SCMD_1, read32(DI_SCMD_1) ^ GCAMKeyB );
						write32( DI_SCMD_2, read32(DI_SCMD_2) ^ GCAMKeyC );

						//Update keys
						u32 seed = read32(DI_SCMD_0) >> 16;

						GCAMKeyA = GCAMKeyA * seed;
						GCAMKeyB = GCAMKeyB * seed;
						GCAMKeyC = GCAMKeyC * seed;

						DIcommand = read32(DI_SCMD_0) & 0xFF;
						break;
				}
			} else {
				DIcommand = read32(DI_SCMD_0) >> 24;
			}
			switch( DIcommand )
			{
				default:
				{
					dbgprintf("DI: Unknown command:%02X\r\n", DIcommand );

					for( i = 0; i < 0x30; i+=4 )
						dbgprintf("0x%08X:0x%08X\t0x%08X\r\n", i, read32( DI_BASE + i ), read32( DI_SHADOW + i ) );
					dbgprintf("\r\n");

					memset32( (void*)DI_BASE, 0xdeadbeef, 0x30 );
					memset32( (void*)(DI_SHADOW), 0, 0x30 );

					write32( DI_SCONFIG, 0xFF );
					write32( DI_SCOVER, 0 );

					Shutdown();

				} break;
				case 0x12:
				{
					#ifdef DEBUG_GCAM
					dbgprintf("GC-AM: 0x12\n");
					#endif
					write32( DI_SIMM, 0x21000000 );

					DIOK = 2;
				} break;
				case 0xAA:
				{
					u32 Buffer	= P2C(read32(DI_SDMA_ADR));
					u32 Length	= read32(DI_SCMD_2);
					u32 Offset	= read32(DI_SCMD_1) << 2;
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
					// Max GC disc offset
					if( Offset >= 0x57058000 )
					{
						dbgprintf("Unhandled Media Board Write\n");
						dbgprintf("GCAM:Write( 0x%08x, 0x%08x, 0x%08x )\n", Offset, Length, Buffer|0x80000000 );
						Shutdown();
					}
					DIOK = 2;
				} break;
				case 0xAB:
				{
					memset32( MediaBuffer, 0, 0x20 );

					MediaBuffer[0] = MediaBuffer[0x20];

					// Command
					*(u16*)(MediaBuffer+2) = *(u16*)(MediaBuffer+0x22) | 0x80;

					u16 cmd = bs16(*(u16*)(MediaBuffer+0x22));

					#ifdef DEBUG_GCAM
					if(cmd)
						dbgprintf("GCAM:Execute command:%03X\n", cmd );
					#endif
					switch( cmd )
					{
						// ?
						case 0x000:
							*(u32*)(MediaBuffer+4) = bs32(1);
							break;
						// DIMM size
						case 0x001:
							*(u32*)(MediaBuffer+4) = bs32(0x20000000);
							break;
						// Media board status
						case 0x100:
							// Status
							*(u32*)(MediaBuffer+4) = bs32(5);
							// Progress in %
							*(u32*)(MediaBuffer+8) = bs32(100);
						break;
						// Media board version: 3.03
						case 0x101:
							// Version
							*(u16*)(MediaBuffer+4) = 0x0303;
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
							memcpy( MediaBuffer + 4, "A89E28A48984511", 15 );
						break;
						case 0x104:
							MediaBuffer[4] = 1;
						break;
						// Get IP (?)
						case 0x406:
						{
							char *IP			= (char*)(NetworkCMDBuffer + ( bs32(*(u32*)(MediaBuffer+0x28)) - 0x1F800200 ) );
							u32 IPLength	= bs32(*(u32*)(MediaBuffer+0x2C));

							memcpy( MediaBuffer + 4, IP, IPLength );

							#ifdef DEBUG_GCAM
							dbgprintf( "GC-AM: Get IP:%s\n", (char*)(MediaBuffer + 4) );
							#endif
						} break;
						// Set IP
						case 0x415:
						{
							#ifdef DEBUG_GCAM
							char *IP			= (char*)(NetworkCMDBuffer + ( bs32(*(u32*)(MediaBuffer+0x28)) - 0x1F800200 ) );
							u32 IPLength	= bs32(*(u32*)(MediaBuffer+0x2C));
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
					write32( DI_SIMM, 0x66556677 );

					DIOK = 2;
				} break;
				case 0xE1:	// play Audio Stream
				{
					switch( (read32(DI_SCMD_0) >> 16 ) & 0xFF )
					{
						case 0x00:
						{
							if( read32(DI_SCMD_1) == 0 && read32(DI_SCMD_2) == 0 )
							{
								StreamStopEnd = 1;						
								dbgprintf("DIP:DVDPrepareStreamAbsAsync( %08X, %08X )\r\n", read32(DI_SCMD_1), read32(DI_SCMD_2) );
							}
							else
							{
								StreamDiscOffset= read32(DI_SCMD_1)<<2;
								StreamSize		= read32(DI_SCMD_2);
								StreamOffset	= 0;
								Streaming		= 1;
								StreamStopEnd	= 0;
								StreamTimer		= read32(HW_TIMER);

								dbgprintf("DIP:Streaming %ds of audio...\r\n", StreamSize / 32 * 28 / 48043 );
								dbgprintf("DIP:Size:%u\r\n", StreamSize );
								dbgprintf("DIP:Samples:%u\r\n", StreamSize / (SAMPLES_PER_BLOCK*sizeof(u16)) );
#ifdef AUDIOSTREAM
								u32 read;
								f_lseek( &GameFile, StreamDiscOffset );

								#ifdef DEBUG_DI
								u32 ret = f_read( &GameFile, (void*)(StreamBuffer+0x1000), StreamSize, &read );
								#else
								f_read( &GameFile, (void*)(StreamBuffer+0x1000), StreamSize, &read );
								#endif

								if( read != StreamSize )
								{
									dbgprintf("DIP:Failed to read:%u(%u) Error:%u\r\n", StreamSize, read, ret );
									Shutdown();
								}

								u32 SrcOff = 0x1000;
								u32	DstOff = 0;

								unsigned int samples = StreamSize / 32;

								while(samples)
								{
									transcode_frame( (char*)(StreamBuffer + SrcOff), 0, (char*)(StreamBuffer + DstOff) );
									DstOff += 16;

									transcode_frame( (char*)(StreamBuffer + SrcOff), 1, (char*)(StreamBuffer + DstOff) );
									SrcOff += ONE_BLOCK_SIZE;
									DstOff += 16;

									samples--;

								//	decode_ngc_dtk( (u8*)(0x11000000 + SrcOff), (u16*)(StreamBuffer + DstOff), 1, 0, 28, 0 );
								//	decode_ngc_dtk( (u8*)buf+i*32, (s16*)(StreamBuffer + DstOff + 2), 2, 0, 28, 1 );

								//	DecodeBlock( (s16*)(StreamBuffer + DstOff), (u8*)(StreamBuffer + 0x1300000 + SrcOff) );

									if( DstOff >= StreamBufferSize )
										break;
								}

								uint8_t *header = (uint8_t*)0x11200000;
								memset32(header, 0, 64);

								// 0-3: sample count
								*(vu32*)(header+0) = samples;

								// 4-7: nibble count
								*(vu32*)(header+4) = samples/14*16;

								// 8-11: sample rate
								*(vu32*)(header+8) = 48000;

								// 12-13: loop flag (0)

								// 14-15: format (0: ADPCM)

								// 16-19: loop start, 20-23: loop end (0)

								// 24-27: ca ("current" nibble offset)
								*(vu32*)(header+24) = 2;

								// 28-59 filter coefficients
								{
									// these are the fixed filters used by XA
									const uint16_t coef[16] = {
										0,      0,
										0x3c,   0,
										0x73,  -0x34,
										0x62,  -0x37
									};

									u32 j;
									for (j = 0; j < 16; j+=2)
									{
										u32 val = coef[j+1]<<5;
											val|= (coef[j]<<5) << 16;

										*(vu32*)(header+28+j*2) = val;
									}
								}
								*(vu32*)(header+60) = *(vu32*)(StreamBuffer)<<16;

								//sync_after_write( (void*)StreamBuffer, StreamBufferSize );

								//sync_before_read( (void*)0, 0x20 );

								*(vu32*)(0x14) = StreamBuffer|0xD0000000;
								*(vu32*)(0x18) = (StreamBuffer+DstOff)|0xD0000000;

								//sync_after_write( (void*)0, 0x20 );
#endif
								
								dbgprintf("DIP:Streaming %ds of audio...\r\n", StreamSize / 32 * 28 / 48043 );  
								dbgprintf("DIP:DVDPrepareStreamAbsAsync( %08X, %08X )\r\n", StreamDiscOffset, StreamSize );
							}
						} break;
						case 0x01:
						{
							StreamDiscOffset= 0;
							StreamSize		= 0;
							StreamOffset	= 0;
							Streaming		= 0;
							
							dbgprintf("DIP:DVDCancelStreamAsync()\r\n");
						} break;
						default:
						{
							dbgprintf("DIP:DVDStream(%d)\r\n", (read32(DI_SCMD_0) >> 16 ) & 0xFF );
						} break;
					} 
					
					DIOK = 2;

				} break;
				case 0xE2:	// request Audio Status
				{
					switch( read32(DI_SCMD_0)<<8 )
					{
						case 0x00000000:	// Streaming?
						{
							write32( DI_SIMM, Streaming );
						} break;
						case 0x01000000:	// What is the current address?
						{
							dbgprintf("DIP:StreamInfo:Cur:%08X End:%08X\r\n", StreamOffset, StreamSize );
							write32( DI_SIMM, ((StreamDiscOffset+StreamOffset) >> 2) & (~0x1FFF) );
						} break;
						case 0x02000000:	// disc offset of file
						{
							write32( DI_SIMM, StreamDiscOffset>>2 );
						} break;
						case 0x03000000:	// Size of file
						{
							write32( DI_SIMM, StreamSize );
						} break;
					}

				//	dbgprintf("DIP:DVDLowAudioGetConfig( %d, %08X )\r\n", (read32(DI_SCMD_0)>>16)&0xFF, read32(DI_SIMM) );

					DIOK = 2;

				} break;
				case 0xE3:	// stop Motor
				{
					dbgprintf("DIP:DVDLowStopMotor()\r\n");
					u32 CDiscNumber = (read32(4) << 16 ) >> 24;
					dbgprintf("DIP:Current disc number:%u\r\n", CDiscNumber + 1 );

					DIChangeDisc( CDiscNumber ^ 1 );

					DiscChangeIRQ = 1;
					
					DIOK = 2;

				} break;
				case 0xE4:	// DVD Audio disable
				{
					DIOK = 2;
				
				} break;
				case 0xA7:
				case 0xA9:
					//dbgprintf("DIP:Async!\r\n");
				case 0xA8:
				{
					u32 Buffer	= P2C(read32(DI_SDMA_ADR));
					u32 Length	= read32(DI_SCMD_2);
					u32 Offset	= read32(DI_SCMD_1) << 2;

					//dbgprintf( "DIP:DVDRead%02X( 0x%08x, 0x%08x, 0x%08x )\r\n", DIcommand, Offset, Length, Buffer|0x80000000 );

					if( (Offset & 0x8FFF0000) == 0x80000000 )
					{
						switch(Offset)
						{
							// Media board status (1)
							case 0x80000000:
								write16( Buffer, 0x0100 );
								break;
							// Media board status (2)
							case 0x80000020:
								memset( (void*)Buffer, 0, Length );
								break;
							// Media board status (3)
							case 0x80000040:
								memset( (void*)Buffer, 0xFFFFFFFF, Length );
								// DIMM size (512MB)
								write32( Buffer, 0x00000020 );
								// GCAM signature
								write32( Buffer+4, 0x4743414D );
								break;
							// Firmware status (1)
							case 0x80000120:
								write32( Buffer, 0x00000001 );
								break;
							// Firmware status (2)
							case 0x80000140:
								write32( Buffer, 0x01000000 );
								break;
							default:
								dbgprintf("Unhandled Media Board Read:%08X\n", Offset );
								break;
						}
						DIOK = 2;
					}  // DIMM memory (3MB)
					else if( (Offset >= 0x1F000000) && (Offset <= 0x1F300000) )
					{
						u32 roffset = Offset - 0x1F000000;
						memcpy( (void*)Buffer, DIMMMemory + roffset, Length );

						DIOK = 2;
					}  // DIMM command
					else if( (Offset >= 0x1F900000) && (Offset <= 0x1F900040) )
					{
						u32 roffset = Offset - 0x1F900000;
					#ifdef DEBUG_GCAM
						dbgprintf("GC-AM: Read MEDIA BOARD COMM AREA (%08x)\n", roffset );
					#endif
						memcpy( (void*)Buffer, MediaBuffer + roffset, Length );

						DIOK = 2;
					}  // Network command
					else if( (Offset >= 0x1F800200) && (Offset <= 0x1F8003FF) )
					{
						u32 roffset = Offset - 0x1F800200;
					#ifdef DEBUG_GCAM
						dbgprintf("GC-AM: Read MEDIA BOARD NETWORK COMMAND (%08x)\n", roffset );
					#endif
						memcpy( (void*)Buffer, NetworkCMDBuffer + roffset, Length );

						DIOK = 2;
					}
					else
					{
						// Max GC disc offset
						if( Offset >= 0x57058000 )
						{
							dbgprintf("Unhandled Read\n");
							dbgprintf( "DIP:DVDRead%02X( 0x%08x, 0x%08x, 0x%08x )\n", DIcommand, Offset, Length, Buffer|0x80000000 );
							Shutdown();
						}
						if( Buffer < 0x01800000 )
						{
							DI_CallbackMsg.result = -1;
							sync_after_write(&DI_CallbackMsg, 0x20);
							IOS_IoctlAsync( DI_Handle, 0, (void*)Offset, 0, (void*)Buffer, Length, DI_MessageQueue, &DI_CallbackMsg );
							DIOK = 2;
						}
					}

					// not async so wait
					if( DIcommand == 0xA8 )
					{
						while(DI_CallbackMsg.result)
						{
							udelay(100);
							CheckOSReport();
						}
						DI_CallbackMsg.result = -1;
						sync_after_write(&DI_CallbackMsg, 0x20);
						DIOK = 1;
					}
				} break;
				case 0xF9:
				{
					CacheInit(FSTBuf, false);
					DIOK = 1;
				}
			}

			if( DIOK )
			{
				//write32( DI_SDMA_LEN, 0 );

				if( DIOK == 2 )
					DI_IRQ = true;
				else
				{
					set32( DI_SSTATUS, 0x3A );
					write32(DI_SCONTROL, 0);
				}
				//while( read32(DI_SCONTROL) & 1 )
				//	clear32( DI_SCONTROL, 1 );
			}
		}
		sync_after_write( (void*)DI_BASE, 0x60 );
	}
	return;
}

static u8 *DI_Read_Buffer = (u8*)(0x11200000);
u32 DIReadThread(void *arg)
{
	//dbgprintf("DI Thread Running\r\n");
	u8 *src = NULL; char *dest = NULL; u32 length = 0, offset = 0;
	struct ipcmessage *di_msg = NULL;
	while(1)
	{
		mqueue_recv( DI_MessageQueue, &di_msg, 0 );
		switch( di_msg->command )
		{
			case IOS_OPEN:
				if( strncmp("/dev/di", di_msg->open.device, 8 ) != 0 )
				{	//this should never happen
					mqueue_ack( di_msg, -6 );
					break;
				}
				f_close( &GameFile );
				s32 ret = f_open( &GameFile, ConfigGetGamePath(), FA_READ|FA_OPEN_EXISTING );
				if( ret != FR_OK )
				{
					_sprintf( GamePath, "%s", ConfigGetGamePath() );
					//Try to switch to FST mode
					if( !FSTInit(GamePath) )
					{
						dbgprintf("Failed to open:%s Error:%u\r\n", ConfigGetGamePath(), ret );	
						Shutdown();
					}
				}
				else
				{
					DWORD tmp = 1; //size 1 to get the real size
					GameFile.cltbl = &tmp;
					f_lseek(&GameFile, CREATE_LINKMAP);
					GameFile.cltbl = malloc(tmp * sizeof(DWORD));
					GameFile.cltbl[0] = tmp; //fatfs automatically sets real size into tmp
					f_lseek(&GameFile, CREATE_LINKMAP); //actually create it
					DIReadISO();
				}
				mqueue_ack( di_msg, 24 );
				break;

			case IOS_CLOSE:
				if( FSTMode )
					FSTCleanup();
				else
				{
					f_close( &GameFile );
					free(GameFile.cltbl);
					GameFile.cltbl = NULL;
				}
				mqueue_ack( di_msg, 0 );
				break;

			case IOS_IOCTL:
				src = DI_Read_Buffer;
				dest = (char*)di_msg->ioctl.buffer_io;
				length = di_msg->ioctl.length_io;
				offset = (u32)di_msg->ioctl.buffer_in;

				if( FSTMode )
					FSTRead( GamePath, src, length, offset );
				else
					src = CacheRead( src, length, offset );

				memcpy( dest, src, length );
				DoPatches( dest, length, offset );
				sync_after_write( dest, length );
				mqueue_ack( di_msg, 0 );
				break;

			case IOS_ASYNC:
				mqueue_ack( di_msg, 0 );
				break;
		}
	}
}
