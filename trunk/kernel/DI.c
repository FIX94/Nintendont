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

extern FIL GameFile;

static char GamePath[256] ALIGNED(32);

extern u32 Region;
extern u32 FSTMode;

void DIinit( void )
{
	u32 read;

	dbgprintf("DIInit()\r\n");

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
	} else {
		f_lseek( &GameFile, 0x458 );
		f_read( &GameFile, &Region, sizeof(u32), &read );
	}

	memset32( (void*)DI_BASE, 0xdeadbeef, 0x30 );
	memset32( (void*)DI_SHADOW, 0, 0x30 );

	sync_after_write( (void*)DI_BASE, 0x60 );

	write32( DIP_IMM, 0 ); //reset errors
	write32( DIP_STATUS, 0x2E );
	write32( DIP_CMD_0, 0xE3000000 ); //spam stop motor
}
void DIChangeDisc( u32 DiscNumber )
{
	f_close( &GameFile );

	u32 read, i;
	char str[256] __attribute__((aligned(0x20)));

	memset32( str, 0, 256 );

	_sprintf( str, "%s", ConfigGetGamePath() );
			
	//search the string backwards for '/'
	for( i=strlen(str); i > 0; --i )
		if( str[i] == '/' )
			break;
	i++;

	if( DiscNumber == 0 )
		_sprintf( str+i, "game.iso" );
	else
		_sprintf( str+i, "disc2.iso" );

	dbgprintf("New Gamepath:\"%s\"\r\n", str );
	
	s32 ret = f_open( &GameFile, str, FA_READ|FA_OPEN_EXISTING );
	if( ret  != FR_OK )
	{
		dbgprintf("Failed to open:%s Error:%u\r\n", str, ret );
		Shutdown();
	}

	f_lseek( &GameFile, 0 );
	f_read( &GameFile, (void*)0, 0x20, &read );

	f_lseek( &GameFile, 0 );
	f_read( &GameFile, str, 0x400, &read );

	dbgprintf("DIP:Loading game %.6s: %s\r\n", str, (char *)(str+0x20));

	f_lseek( &GameFile, 0x420 );
	f_read( &GameFile, str, 0x40, &read );

	free(str);

}
void DIUpdateRegisters( void )
{
	u32 read,i;
	u32 DIOK = 0,DIcommand;

	u32 *DInterface	 = (u32*)(DI_BASE);
	u32 *DInterfaceS = (u32*)(DI_SHADOW);
	
	sync_before_read( (void*)DI_BASE, 0x60 );

	if( read32(DI_CONTROL) != 0xdeadbeef )
	{
		write32( DI_SCONTROL, read32(DI_CONTROL) & 3 );

		*(vu32*)DI_SSTATUS &= ~0x14;

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
			DIcommand = read32(DI_SCMD_0) >> 24;
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
							} else {

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
								f_lseek( &GameFile, StreamDiscOffset );
								ret = f_read( &GameFile, (void*)(StreamBuffer+0x1000), StreamSize, &read );

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
								memset32(header, 0, sizeof(header));

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

					write32( HW_TIMER, 0 );

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
					dbgprintf( "DIP:DVDRead%02X( 0x%08x, 0x%08x, 0x%08x )\r\n", read32(DI_SCMD_0) >> 24, Offset, Length, Buffer|0x80000000 );
					memset32((void*)0x12100000, 0, Length);
					if( FSTMode )
						FSTRead( GamePath, (char*)0x12100000, Length, Offset );
					else
					{
						if( GameFile.fptr != Offset )
							f_lseek( &GameFile, Offset );

						s32 ret = f_read( &GameFile, (void*)0x12100000, Length, &read );
				//		dbgprintf( "%d\r\n", read );
						if( ret != FR_OK )
						{
							dbgprintf( "f_read failed(%u,%u):%d\r\n", Length, read, ret );
							Shutdown();
						}
					}
					memcpy((void*)Buffer, (void*)0x12100000, Length);
					DoPatches( (char*)Buffer, Length, Offset );
					sync_after_write( (void*)Buffer, Length );
					if( DIcommand == 0xA7 )
					{
						DIOK = 2;
					} else {
						DIOK = 1;
					}
				}
			}

			if( DIOK )
			{
				//write32( DI_SDMA_LEN, 0 );
				set32( DI_SSTATUS, 0x3A );
				sync_after_write( (void*)DI_BASE, 0x60 );

				if( DIOK == 2 )
				{
					//wait_for_ppc(5); //wait so game expects it
					write32( DIP_CONTROL, 1 ); //start transfer so game gets its data
					while( read32(DIP_CONTROL) & 1 ) ;
				}
				write32(DI_SCONTROL, 0);
				//while( read32(DI_SCONTROL) & 1 )
				//	clear32( DI_SCONTROL, 1 );
			}
		}
		sync_after_write( (void*)DI_BASE, 0x60 );
	}
	else //give the hid thread some time
		udelay(10);
	return;
}