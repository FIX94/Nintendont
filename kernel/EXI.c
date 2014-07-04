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
#include "global.h"
#include "EXI.h"
#include "ff.h"
#include "common.h"
#include "vsprintf.h"
#include "alloc.h"
#include "Patch.h"
#include "syscalls.h"
#include "Config.h"
#include "debug.h"

extern u8 SRAM[64];
extern u32 Region;

u32 Device=0;
u32 SRAMWriteCount=0;
static u32 EXICommand = 0;
static u32 BlockOff= 0;
static bool changed = false;
static u32 BlockOffLow = 0xFFFFFFFF;
static u32 BlockOffHigh = 0x00000000;
u8 *MCard = (u8 *)(0x11000000);
u8 *FontBuf = (u8 *)(0x13100000);
u32 CARDWriteCount = 0;
u32 IPLReadOffset;
FIL MemCard;
bool SkipHandlerWait = false;
bool EXI_IRQ = false;
static u32 IRQ_Timer = 0;
static u32 IRQ_Cause = 0;
static u32 IRQ_Cause2= 0;
static TCHAR MemCardName[0x20];

static u32 TRIBackupOffset= 0;
static u32 EXI2IRQ				= 0;
static u32 EXI2IRQStatus	= 0;

void EXIInit( void )
{
	dbgprintf("EXIInit Start\r\n");
	u32 i, wrote, ret;
	
	write32( 0x0D80600C, 0 );
	write32( 0x0D806010, 0 );

	if( ConfigGetConfig(NIN_CFG_MEMCARDEMU) )
	{
		f_chdir("/saves");
		u32 GameID = ConfigGetGameID();
		memset32(MemCardName, 0, 0x20);
		memcpy(MemCardName, &GameID, 4);
		memcpy(MemCardName+4, ".raw", 4);
		sync_after_write(MemCardName, 0x20);

		dbgprintf("Trying to open %s\r\n", MemCardName);
		ret = f_open( &MemCard, MemCardName, FA_READ );
		if( ret != FR_OK || MemCard.fsize == 0 )
		{
#ifdef DEBUG_EXI
			dbgprintf("EXI: Failed to open %s:%u\r\n", MemCardName, ret );
#endif
			Shutdown();
		}

#ifdef DEBUG_EXI
		dbgprintf("EXI: Loading memory card...");
#endif

		f_lseek( &MemCard, 0 );
		f_read( &MemCard, MCard, NIN_RAW_MEMCARD_SIZE, &wrote );
		f_close( &MemCard );
#ifdef DEBUG_EXI
		dbgprintf("done\r\n");
#endif
		sync_after_write( MCard, NIN_RAW_MEMCARD_SIZE );
	}

	GC_SRAM *sram = (GC_SRAM*)SRAM;
	
	for( i=0; i < 12; ++i )
		sram->FlashID[0][i] = 0;

	sram->FlashIDChecksum[0] = 0xFF;

	sram->DisplayOffsetH = 0;
	sram->BootMode	&= ~0x40;	// Clear PAL60
	sram->Flags		&= ~3;		// Clear Videomode
	sram->Flags		&= ~0x80;	// Clear Progmode

	sram->Language = ConfigGetLanguage();

	if( ConfigGetVideoMode() & NIN_VID_FORCE )
	{
		switch( ConfigGetVideoMode() & NIN_VID_FORCE_MASK )
		{
			case NIN_VID_FORCE_NTSC:
			{
				Region = 0;
			} break;
			case NIN_VID_FORCE_MPAL:
			case NIN_VID_FORCE_PAL50:
			case NIN_VID_FORCE_PAL60:
			{
				Region = 2;
			} break;
		}
	}

	switch(Region)
	{
		default:
		case 0:
		case 1:
		{
#ifdef DEBUG_EXI
			dbgprintf("SRAM:NTSC\r\n");
#endif
			*(vu32*)0xCC = 0;
			
		} break;
		case 2:
		{
			if( *(vu32*)0xCC == 5 )
			{
#ifdef DEBUG_EXI
				dbgprintf("SRAM:PAL60\r\n");
#endif
			} else {
#ifdef DEBUG_EXI
				dbgprintf("SRAM:PAL50\r\n");
#endif
				*(vu32*)0xCC = 1;
			}
			sram->Flags		|= 1;
			sram->BootMode	|= 0x40;

		} break;
	}
	if( ConfigGetVideoMode() & NIN_VID_PROG )
		sram->Flags |= 0x80;
	else
		sram->Flags &= 0x7F;

	SRAM_Checksum( (unsigned short *)SRAM, (unsigned short *)SRAM, (unsigned short *)( ((u8*)SRAM) + 2 ) );
}
bool EXICheckTimer(void)
{
	return (read32(HW_TIMER) - IRQ_Timer) >= 3800;	// about 500 times a second
}
void EXIInterrupt(void)
{
	write32( 0x10, IRQ_Cause );
	write32( 0x18, IRQ_Cause2 );
	write32( 0x14, 0x10 );		// EXI IRQ
	sync_after_write( (void*)0, 0x20 );

	if(SkipHandlerWait == true)
		write32( HW_IPC_ARMCTRL, (1<<0) | (1<<4) ); //throw irq
	else while(read32(0x14) == 0x10)
	{
		write32( HW_IPC_ARMCTRL, (1<<0) | (1<<4) ); //throw irq
		wait_for_ppc(1);
		sync_before_read((void*)0x14, 4);
	}

	EXI_IRQ = false;
	IRQ_Timer = 0;
	IRQ_Cause = 0;
}
bool EXICheckCard(void)
{
	if(changed == true)
	{
		changed = false;
		return true;
	}
	return false;
}

void EXISaveCard(void)
{
	u32 wrote;
	
	if (BlockOffLow < BlockOffHigh)
	{
//#ifdef DEBUG_EXI
		//dbgprintf("EXI: Saving memory card...");
//#endif
		s32 ret = f_open( &MemCard, MemCardName, FA_WRITE );
		if( ret == FR_OK )
		{
			sync_before_read(MCard, NIN_RAW_MEMCARD_SIZE);
			f_lseek(&MemCard, BlockOffLow);
			f_write(&MemCard, MCard + BlockOffLow, BlockOffHigh - BlockOffLow, &wrote);
			f_close(&MemCard);
//#ifdef DEBUG_EXI
			//dbgprintf("Done!\r\n");
		}
		//else
			//dbgprintf("\r\nUnable to open memory card file:%u\r\n", ret );
//#endif
		BlockOffLow = 0xFFFFFFFF;
		BlockOffHigh = 0x00000000;
	}
}

void EXIShutdown( void )
{
	u32 wrote;

//#ifdef DEBUG_EXI
	dbgprintf("EXI: Saving memory card...");
//#endif

	sync_before_read( MCard, NIN_RAW_MEMCARD_SIZE );
	s32 ret = f_open( &MemCard, MemCardName, FA_WRITE );
	if( ret == FR_OK )
	{
		f_lseek( &MemCard, 0 );
		f_write( &MemCard, MCard, NIN_RAW_MEMCARD_SIZE, &wrote );
		f_close( &MemCard );
	}
	else
		dbgprintf("\r\nUnable to open memory card file:%u\r\n", ret );
//#ifdef DEBUG_EXI
	dbgprintf("Done!\r\n");
//#endif
}
u32 EXIDeviceMemoryCard( u8 *Data, u32 Length, u32 Mode )
{
	u32 EXIOK = 1;
	//u32 read, wrote;

	if( Mode == 1 )		// Write
	{
		switch( Length )
		{
			case 1:
			{
				if( EXICommand == MEM_BLOCK_READ || EXICommand == MEM_BLOCK_WRITE )
					break;

				switch( (u32)Data >> 24 )
				{
					case 0x00:
					{
						EXICommand = MEM_READ_ID_NINTENDO;
#ifdef DEBUG_EXI
						dbgprintf("EXI: CARDGetDeviceIDNintendo()\r\n");
#endif
					} break;
#ifdef DEBUG_EXI					
					case 0x89:
					{
						dbgprintf("EXI: CARDClearStatus()\r\n");
					} break;
#endif
				}
			} break;
			case 2:
			{
				switch( (u32)Data >> 16 )
				{
					case 0x0000:
					{
						EXICommand = MEM_READ_ID;
#ifdef DEBUG_EXI
						dbgprintf("EXI: CARDGetDeviceID()\r\n");
#endif
					} break;
					case 0x8300:	//
					{
						EXICommand = MEM_READ_STATUS;
#ifdef DEBUG_EXI
						dbgprintf("EXI: CARDReadStatus()\r\n");
#endif
					} break;
#ifdef DEBUG_EXI
					case 0x8101:
					{
						dbgprintf("EXI: CARDIRQEnable()\r\n");
					} break;
					case 0x8100:
					{
						dbgprintf("EXI: CARDIRQDisable()\r\n");
					} break;
#endif
				}
			} break;
			case 3:
			{
				switch( (u32)Data >> 24 )
				{
					case 0xF1:
					{
						BlockOff = (((u32)Data>>16)&0xFF)  << 17;
						BlockOff|= (((u32)Data>> 8)&0xFF)  << 9;
#ifdef DEBUG_EXI
						dbgprintf("EXI: CARDErasePage(%08X)\r\n", BlockOff );
#endif
						EXICommand = MEM_BLOCK_ERASE;
						CARDWriteCount = 0;
						IRQ_Cause = 2;			// EXI IRQ
						EXIOK = 2;
					} break;
				}
			} break;
			case 4:
			{
				if( EXICommand == MEM_BLOCK_READ || EXICommand == MEM_BLOCK_WRITE )
					break;

				switch( (u32)Data >> 24 )
				{
					case 0xF1:
					{
						BlockOff = (((u32)Data>>16)&0xFF)  << 17;
						BlockOff|= (((u32)Data>> 8)&0xFF)  << 9;
						BlockOff|= (((u32)Data&0xFF) & 3 ) << 7;
#ifdef DEBUG_EXI
						dbgprintf("EXI: CARDErasePage(%08X)\r\n", BlockOff );
#endif
						EXICommand = MEM_BLOCK_ERASE;
						CARDWriteCount = 0;
						IRQ_Cause = 2;			// EXI IRQ
						EXIOK = 2;
					} break;
					case 0xF2:
					{
						BlockOff = (((u32)Data>>16)&0xFF)  << 17;
						BlockOff|= (((u32)Data>> 8)&0xFF)  << 9;
						BlockOff|= (((u32)Data&0xFF) & 3 ) << 7;
#ifdef DEBUG_EXI
						dbgprintf("EXI: CARDWritePage(%08X)\r\n", BlockOff );
#endif
						EXICommand = MEM_BLOCK_WRITE;
					} break;
					case 0x52:
					{
						BlockOff = (((u32)Data>>16)&0xFF)  << 17;
						BlockOff|= (((u32)Data>> 8)&0xFF)  << 9;
						BlockOff|= (((u32)Data&0xFF) & 3 ) << 7;							
						
#ifdef DEBUG_EXI
						dbgprintf("EXI: CARDReadPage(%08X)\r\n", BlockOff );
#endif

						EXICommand = MEM_BLOCK_READ;
					} break;
#ifdef DEBUG_EXI
					default:
					{
						dbgprintf("EXI: Unknown:%08x Line:%u\r\n", (u32)Data, __LINE__ );
					//	Shutdown();
					} break;
#endif
				}			
			} break;
			default:
			{
				switch( EXICommand )
				{
					case MEM_BLOCK_WRITE:
					{
						if(BlockOff < BlockOffLow)
							BlockOffLow = BlockOff;
						if(BlockOff + Length > BlockOffHigh)
							BlockOffHigh = BlockOff + Length;
						changed = true;
						sync_before_read( Data, Length );

						memcpy( MCard+BlockOff, Data, Length );

						sync_after_write( MCard+BlockOff, Length );	

						IRQ_Cause = 10;	// TC(8) & EXI(2) IRQ
						EXIOK = 2;
					} break;
				}
			} break;
		}

	} else {			// Read

		switch( EXICommand )
		{
			case MEM_READ_ID_NINTENDO:
			case MEM_READ_ID:
			{
				if( ConfigGetConfig(NIN_CFG_MEMCARDEMU) )
				{
					write32( 0x0D806010, NIN_MEMCARD_BLOCKS );
				} else {
					write32( 0x0D806010, 0x00000000 ); //no memory card
				}
#ifdef DEBUG_EXI
				dbgprintf("EXI: CARDReadID(%X)\r\n", read32(0x0D806010) );
#endif
			} break;
			case MEM_READ_STATUS:
			{
				write32( 0x0D806010, 0x41 );	// Unlocked(0x40) and Ready(0x01)
#ifdef DEBUG_EXI
				dbgprintf("EXI: CARDReadStatus(%X)\r\n", read32(0x0D806010) );
#endif
			} break;
			case MEM_BLOCK_READ:
			{
			//	f_lseek( &MemCard, BlockOff );
			//	f_read( &MemCard, Data, Length, &read );
				sync_before_read( MCard+BlockOff, Length );

				memcpy( Data, MCard+BlockOff, Length );

				sync_after_write( Data, Length );

				IRQ_Cause = 8;		// TC IRQ

				EXIOK = 2;
			} break;
		}
	}
	//dbgprintf("%08x %08x %08x %08x\r\n", (u32)Data >> 16, Mode, Length, EXICommand);
	write32( 0x0D80600C, 0 ); //exit EXIDMA / EXIImm

	if( EXIOK == 2 )
	{
		EXI_IRQ = true;
		IRQ_Timer = read32(HW_TIMER);
	}
	return 1;
}
u32 EXIDevice_ROM_RTC_SRAM_UART( u8 *Data, u32 Length, u32 Mode )
{
	if( Mode == 1 )		// Write
	{
		switch( Length )
		{
			case 4:
			{
				if( EXICommand == SRAM_WRITE )
				{
					*(u32*)(SRAM+SRAMWriteCount) = (u32)Data;

					SRAMWriteCount += Length;

					break;
				}

				if( (u32)Data == 0x20000100 )
				{
					EXICommand = SRAM_READ;
#ifdef DEBUG_SRAM
					dbgprintf("EXI: SRAMRead()\r\n");
#endif
					break;
				}

				if( (u32)Data == 0xA0000100 || (u32)Data == 0xA0000600 )
				{
					EXICommand = SRAM_WRITE;
					SRAMWriteCount = 0;
#ifdef DEBUG_SRAM
					dbgprintf("EXI: SRAMWrite()\r\n");
#endif
					break;
				}

				if( ((u32)Data >> 6 ) >= IPL_ROM_FONT_SJIS )
				{
					EXICommand = IPL_READ_FONT;
#ifdef DEBUG_SRAM
					//dbgprintf("EXI: IPLReadFont()\r\n");
#endif
					IPLReadOffset = (u32)Data >> 6;
					break;
				}

			} break;
		}

	} else {

		switch( EXICommand )
		{
			case IPL_READ_FONT:
			{
#ifdef DEBUG_SRAM	
				//dbgprintf("EXI: IPLRead( %p, %08X, %u)\r\n", Data, IPLReadOffset, Length );
#endif
				EXIReadFontFile(Data, Length);
			} break;
			case SRAM_READ:
			{
#ifdef DEBUG_SRAM	
				dbgprintf("EXI: SRAMRead(%p,%u)\r\n", Data, Length );
#endif
				memcpy( Data, SRAM, Length );
				sync_after_write( Data, Length );
			} break;
#ifdef DEBUG_SRAM
			default:
			{
				dbgprintf("EXI: Unknown:%08x Line:%u\r\n", (u32)Data, __LINE__ );
				Shutdown();
			} break;
#endif
		}
	}

	write32( 0x0D80600C, 0 );

	return 1;
}
u32 EXIDeviceSP1( u8 *Data, u32 Length, u32 Mode )
{
	u32 EXIOK = 0;

	if( Mode == 1 )		// Write
	{
		switch( Length )
		{
			/*
				0: command
			1-2: arg
			  3: checksum
			*/
			case 4:
			{
				switch( (u32)Data >> 24 )
				{
					case 0x01:
					{
						TRIBackupOffset = ((u32)Data >> 8) & 0xFFFF;
						EXICommand = AMBB_BACKUP_OFFSET;
					} break;
					case 0x02:
					{
						MCard[TRIBackupOffset] = ((u32)Data >> 16) & 0xFF;
						EXICommand = AMBB_BACKUP_WRITE;
					} break;
					case 0x03:
					{
						EXICommand = AMBB_BACKUP_READ;
					} break;
					case 0x82:
					{
#ifdef DEBUG_GCAM
						dbgprintf("EXI:ISRRead()\n");
#endif
						EXICommand = AMBB_ISR_READ;
					} break;
					case 0x83:
					{
#ifdef DEBUG_GCAM
						dbgprintf("EXI:0x83()\n");
#endif
						EXICommand = AMBB_UNKNOWN;
					} break;
					case 0x86:
					{
#ifdef DEBUG_GCAM
						dbgprintf("EXI:IMRRead()\n");
#endif
						EXICommand = AMBB_IMR_READ;
					} break;
					case 0x87:
					{
#ifdef DEBUG_GCAM
						dbgprintf("EXI:IMRWrite(%04X)\n", ((u32)Data >> 8) & 0xFFFF );
#endif
						EXICommand = AMBB_IMR_WRITE;
					} break;
					case 0xFF:
					{
#ifdef DEBUG_GCAM
						dbgprintf("EXI:LANCNTWrite(%04X)\n", ((u32)Data >> 8) & 0xFFFF );
#endif
						if( (((u32)Data >> 8) & 0xFFFF) == 0x0201 )
						{
							EXI2IRQStatus = 0;

						} else if ((((u32)Data >> 8) & 0xFFFF) == 0 )
						{
							EXI2IRQ = 1;
							EXI2IRQStatus = 2;
						}
						EXICommand = AMBB_LANCNT_WRITE;
					} break;
				}
			} break;
			case 2:
			{
				switch( (u32)Data >>16 )
				{
					case 0x0000:
					{
						EXICommand = MEM_READ_ID;
					} break;
				}
			} break;
		}

	} else {
		switch(EXICommand)
		{
			/* 0x02 */
			case AMBB_BACKUP_WRITE:
			/* 0x01 */
			case AMBB_BACKUP_OFFSET:
			{
				write32( 0x0D806010, 0x01 );
			} break;
			/* 0x03 */
			case AMBB_BACKUP_READ:
			{
				write32( 0x0D806010, 0x0100 | MCard[TRIBackupOffset] );
			} break;
			/* 0x87 */
			case AMBB_IMR_WRITE:
			/* 0x83 */
			case AMBB_UNKNOWN:
			{
				write32( 0x0D806010, 0x04 );
			} break;
			/* 0x82 */
			case AMBB_ISR_READ:
			{
				if( Length == 1 )
				{
					write32( 0x0D806010, 0x04 );
				} else {
					write32( 0x0D806010, EXI2IRQStatus );
					EXI2IRQ = 0;
				}
			} break;
			/* 0x86 */
			case AMBB_IMR_READ:
			{
				if( Length == 1 )
				{
					write32( 0x0D806010, 0x04 );
				} else {
					write32( 0x0D806010, 0x0000 );
				}
			} break;
			/* 0xFF */
			case AMBB_LANCNT_WRITE:
			{
				if( EXI2IRQ )
				{
					EXIOK = 2;
				}
				write32( 0x0D806010, 0x04 );
			} break;
			case MEM_READ_ID:
			{
				write32( 0x0D806010, EXI_DEVTYPE_BASEBOARD );
			} break;
		}
	}

	write32( 0x0D80600C, 0 );

	if( EXIOK == 2 )
	{
		IRQ_Cause = 8;
		IRQ_Cause2 = 2 ;
		EXI_IRQ = true;
	}

	return 0;
}
void EXIUpdateRegistersNEW( void )
{
	//uu32 chn, dev, frq, ret, data, len, mode;
	u32 chn, dev, ret, data, len, mode;
	u8 *ptr;
	
	u32 command = read32(0x0D80600C);
	
	if( command & 0xFF000000 )
	{
		switch( command >> 24 )
		{
			case 0x10:	// EXISelect
			{
				chn = command & 0xFF;
				dev = (command>>8) & 0xFF;
				//frq = (command>>16) & 0xFF;
				
				//dbgprintf("EXISelect( %u, %u, %u )\r\n", chn, dev, frq );
				ret = 1;

				switch( chn )
				{
					case 0:
					{
						switch( dev )
						{
							case 0:
							{
								Device = EXI_DEV_MEMCARD_A;
							} break;
							case 1:
							{
								Device = EXI_DEV_MASK_ROM_RTC_SRAM_UART;
							} break;
							case 2:
							{
								Device = EXI_DEV_SP1;
							} break;
						}
					} break;
					case 1:
					{
						Device = EXI_DEV_MEMCARD_B;
						ret = 0;
					} break;
					case 2:
					{
						Device = EXI_DEV_AD16;
						ret = 0;
					} break;
				}

				EXICommand = 0;
				
				write32( 0x0D806010, ret );
				write32( 0x0D80600C, 0 );

			} break;
			case 0x11:	// EXI_Imm( s32 nChn, void *pData, u32 nLen, u32 nMode, EXICallback tc_cb );
			{
				chn	=	(command >> 20) & 0xF;
				data=	read32(0x0D806010);
				len	=	command& 0xFFFF;
				mode=	(command >> 16) & 0xF;

				if( len > 4 )
				{
					data = P2C(data);
				}
				
				//dbgprintf("EXIImm( %u, %p, %u, %u, Dev:%u EC:%u )\r\n", chn, data, len, mode, Device, EXICommand );
				switch( Device )
				{
					case EXI_DEV_MEMCARD_A:
					{
						EXIDeviceMemoryCard( (u8*)data, len, mode );
					} break;
					case EXI_DEV_MASK_ROM_RTC_SRAM_UART:
					{
						EXIDevice_ROM_RTC_SRAM_UART( (u8*)data, len, mode );
					} break;
					case EXI_DEV_SP1:
					{
						EXIDeviceSP1( (u8*)data, len, mode );
					} break;
					default:
					{
#ifdef DEBUG_SRAM
						dbgprintf("EXI: EXIImm: Unhandled device:%u\r\n", Device );
#endif
					} break;
				}

			} break;
			case 0x12:	// EXIDMA
			{
				chn	=	(command >> 20) & 0xF;
				ptr=	(u8*)P2C(read32(0x0D806010));
				len	=	command& 0xFFFF;
				mode=	(command >> 16) & 0xF;
				
				//dbgprintf("EXIDMA( %u, %p, %u, %u )\r\n", chn, ptr, len, mode );
				switch( Device )
				{
					case EXI_DEV_MEMCARD_A:
					{
						EXIDeviceMemoryCard( ptr, len, mode );
					} break;
					case EXI_DEV_MASK_ROM_RTC_SRAM_UART:
					{
						EXIDevice_ROM_RTC_SRAM_UART( ptr, len, mode );
					} break;
					case EXI_DEV_SP1:
					{
#ifdef DEBUG_SRAM
						hexdump( ptr, len );
#endif
						EXIDeviceSP1( ptr, len, mode );
					} break;
					default:
					{
#ifdef DEBUG_SRAM
						dbgprintf("EXI: EXIDMA: Unhandled device:%u\r\n", Device );
#endif
					} break;
				}

				EXICommand = 0;

			} break;
			default:
			{
			} break;
		}
	}
}

// FontBuf starts at IPL_ROM_FONT_SJIS
void EXIReadFontFile(u8* Data, u32 Length)
{
	memcpy(Data, FontBuf + IPLReadOffset - IPL_ROM_FONT_SJIS, Length);
	sync_after_write(Data, Length);
}
