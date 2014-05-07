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

//#define DEBUG_EXI 1
//#define DEBUG_SRAM 1

extern u8 SRAM[64];
extern u32 Region;

u32 Device=0;
u32 SRAMWriteCount=0;
static u32 EXICommand = 0;
static u32 BlockOff= 0;
static u32 BlockOffLow = 0xFFFFFFFF;
static u32 BlockOffHigh = 0x00000000;
u8 *MCard = (u8 *)(0x11000000);
u32 CARDWriteCount = 0;
u32 IPLReadOffset;
FIL MemCard;
bool SkipHandlerWait = false;

static TCHAR MemCardName[9];
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
		memset32(MemCardName, 0, 9);
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
				dbgprintf("SRAM:PAL60\n");
#endif
			} else {
#ifdef DEBUG_EXI
				dbgprintf("SRAM:PAL50\n");
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

bool EXICheckCard(void)
{
	if(BlockOffLow != 0xFFFFFFFF)
		return true;
	
	return false;
}

void EXISaveCard(void)
{
	u32 wrote;
	
	if (BlockOffLow < BlockOffHigh)
	{
//#ifdef DEBUG_EXI
		dbgprintf("EXI: Saving memory card...");
//#endif
		s32 ret = f_open( &MemCard, MemCardName, FA_WRITE );
		if( ret == FR_OK )
		{
			sync_before_read(MCard, NIN_RAW_MEMCARD_SIZE);
			f_lseek(&MemCard, BlockOffLow);
			f_write(&MemCard, MCard + BlockOffLow, BlockOffHigh - BlockOffLow, &wrote);
			f_close(&MemCard);
//#ifdef DEBUG_EXI
			dbgprintf("Done!\r\n");
		}
		else
			dbgprintf("Unable to open memory card file:%u\r\n", ret );
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
		dbgprintf("Unable to open memory card file:%u\r\n", ret );
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
						write32( 0x10, 2 );			// EXI IRQ
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
						write32( 0x10, 2 );			// EXI IRQ
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
							
						sync_before_read( Data, Length );

						memcpy( MCard+BlockOff, Data, Length );

						sync_after_write( MCard+BlockOff, Length );	

						write32( 0x10, 10 );	// TC(8) & EXI(2) IRQ
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

				write32( 0x10, 8 );		// TC IRQ

				EXIOK = 2;
			} break;
		}
	}
	//dbgprintf("%08x %08x %08x %08x\r\n", (u32)Data >> 16, Mode, Length, EXICommand);
	write32( 0x0D80600C, 0 ); //exit EXIDMA / EXIImm
	write32(0x13010000,1);
	sync_after_write((void*)0x13010000,4);
	if( EXIOK == 2 )
	{
		write32( 0x14, 0x10 );		// EXI(TC) IRQ
		sync_after_write( (void*)0x14, 4 );
		wait_for_ppc(1);
		if(SkipHandlerWait == true)
			write32( HW_IPC_ARMCTRL, (1<<0) | (1<<4) ); //throw irq
		else
		{
			while(read32(0x13010000) == 1)
			{
				write32( HW_IPC_ARMCTRL, (1<<0) | (1<<4) ); //throw irq
				sync_before_read((void*)0x13010000, 4);
			}
		}
		/*write32( HW_PPCIRQFLAG, read32(HW_PPCIRQFLAG) );
		write32( HW_ARMIRQFLAG, read32(HW_ARMIRQFLAG) );
		set32( HW_IPC_ARMCTRL, (1<<2) );*/
	}

	return 1;
}
u32 EXIDevice_ROM_RTC_SRAM_UART( u8 *Data, u32 Length, u32 Mode )
{
	u32 read;

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

				if( ((u32)Data >> 6 ) >= 0x1FCF00 )
				{
					EXICommand = IPL_READ_FONT_ANSI;
#ifdef DEBUG_SRAM
					dbgprintf("EXI: IPLReadFont()\r\n");
#endif
					IPLReadOffset = (u32)Data >> 6;
					break;
				}

			} break;
		}

	} else {

		switch( EXICommand )
		{
			case IPL_READ_FONT_ANSI:
			{
#ifdef DEBUG_SRAM	
					dbgprintf("EXI: IPLRead( %p, %08X, %u)\r\n", Data, IPLReadOffset, Length );
#endif
				FIL ipl;
				if( f_open( &ipl, "/ipl.bin", FA_OPEN_EXISTING|FA_READ ) == FR_OK )
				{
					f_lseek( &ipl, IPLReadOffset );
					f_read( &ipl, (void*)0x11200000, Length, &read );
					f_close( &ipl );

					memcpy( Data, (void*)0x11200000, Length );
					sync_after_write( Data, Length );
				}
				else if (f_open(&ipl, "/font_ansi.bin", FA_OPEN_EXISTING | FA_READ) == FR_OK)
				{
					f_lseek( &ipl, IPLReadOffset - 0x1FCF00 );
					f_read( &ipl, (void*)0x11200000, Length, &read );
					f_close( &ipl );

					memcpy(Data, (void*)0x11200000, Length);
					sync_after_write(Data, Length);
				}
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
	if( Mode == 1 )		// Write
	{
		switch( Length )
		{
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
			case MEM_READ_ID:
			{
				write32( 0x0D806010, 0 );
			} break;
		}
	}

	write32( 0x0D80600C, 0 );

	return 0;
}
void EXIUpdateRegistersNEW( void )
{
	//u32 chn, dev, frq, ret, data, len, mode;
	u32 chn, dev, ret, data, len, mode;
	u8 *ptr;
	
	u32 command = read32(0x0D80600C);
	
	if( command & 0xFF000000 )
	{
		switch( command >> 24 )
		{
			case 0x10:	// EXISelect
			{
				chn	=  command & 0xFF;
				dev = (command>>8) & 0xFF;
				//frq = (command>>16) & 0xFF;
				
			//	dbgprintf("EXISelect( %u, %u, %u )\r\n", chn, dev, frq );
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
				
			//	dbgprintf("EXIImm( %u, %p, %u, %u, Dev:%u EC:%u )\r\n", chn, data, len, mode, Device, EXICommand );
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
				
		//		dbgprintf("EXIDMA( %u, %p, %u, %u )\r\n", chn, ptr, len, mode );
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

