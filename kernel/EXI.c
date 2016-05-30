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
#include "DI.h"
#include "common.h"
#include "vsprintf.h"
#include "alloc.h"
#include "Patch.h"
#include "Config.h"
#include "debug.h"
#include "SRAM.h"

#include "ff_utf8.h"

#define EXI_IRQ_INSTANT		0		// as soon as possible
#define EXI_IRQ_DEFAULT		1900	// about 1000 times a second
#define EXI_IRQ_SLOW		3800	// about 500 times a second

static u32 CurrentTiming = EXI_IRQ_DEFAULT;

extern vu32 useipl;
static bool exi_inited = false;

// EXI device selection. (per channel)
// idx 3 provided to prevent overflows.
static u8 EXIDeviceSelect[4] = {0, 0, 0, 0};

static u32 SRAMWriteCount = 0;
static u32 EXICommand = 0;
static u8 *const FontBuf = (u8*)(0x13100000);
static u32 IPLReadOffset;
bool EXI_IRQ = false;
static u32 IRQ_Timer = 0;
static u32 IRQ_Cause = 0;
static u32 IRQ_Cause2= 0;

// EXI devices.
// Low 2 bits: Device number. (0-2)
// High 2 bits: Channel number. (0-2)
// Reference: http://hitmen.c02.at/files/yagcd/yagcd/chap10.html
#define EXI_DEVICE_NUMBER(chn, dev) (((chn)<<2)|((dev)&0x3))
enum EXIDevice
{
	EXI_DEV_NONE			= -1,
	EXI_DEV_MEMCARD_A		= EXI_DEVICE_NUMBER(0, 0),
	EXI_DEV_MASK_ROM_RTC_SRAM_UART	= EXI_DEVICE_NUMBER(0, 1),
	EXI_DEV_SP1			= EXI_DEVICE_NUMBER(0, 2),
	EXI_DEV_ETH			= EXI_DEV_SP1,
	EXI_DEV_BASEBOARD		= EXI_DEV_SP1,
	EXI_DEV_MEMCARD_B		= EXI_DEVICE_NUMBER(1, 0),
	EXI_DEV_AD16			= EXI_DEVICE_NUMBER(2, 0),
};

// Memory Card context.
// NOTE: Triforce still accesses this directly instead of
// using the CARD_ctx struct.
static u8 *const CARD_base = (u8*)(0x11000000);

typedef struct _CARD_ctx {
	char filename[0x20];	// Memory Card filename.
	u8 *base;		// Base address.
	u32 size;		// Size, in bytes.
	bool changed;		// True if modified.
	u32 code;		// Memory card "code".

	// NOTE: BlockOff is in bytes, not blocks.
	u32 BlockOff;		// Current offset.
	u32 BlockOffLow;	// Low address of last modification.
	u32 BlockOffHigh;	// High address of last modification.
	u32 CARDWriteCount;	// Write count. (TODO: Is this used anywhere?)
} CARD_ctx;
static CARD_ctx memCard[2] __attribute__((aligned(32)));

static void Init_CARD_ctx(CARD_ctx *ctx)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->BlockOffLow = 0xFFFFFFFF;
}

/**
 * Is a memory card enabled?
 * @param slot Slot number. (0 == Slot A, 1 == Slot B)
 * @return 0 if disabled; 1 if enabled.
 */
static inline u32 isCardEnabled(int slot)
{
	if (slot < 0 || slot > 2)
		return 0;

	// Card is enabled if it's larger than 0 bytes.
	return (memCard[slot].size > 0);
}

extern vu32 TRIGame;
static u32 TRIBackupOffset= 0;
static u32 EXI2IRQ			= 0;
static u32 EXI2IRQStatus	= 0;

static u8 *ambbBackupMem;

/**
 * Load a memory card from disk.
 * @param slot Slot number. (0 == Slot A, 1 == Slot B)
 * NOTE: Slot B is not valid on Triforce.
 * @return 0 on success; non-zero on error.
 */
static int EXILoadCard(int slot)
{
	switch (slot)
	{
		case 0:
			// Slot A
			break;
		case 1:
			// Slot B (not valid on Triforce)
			if (TRIGame != 0)
				return -1;
			break;
		default:
			// Invalid slot.
			return -2;
	}

	// Get the Game ID.
	const u32 GameID = ConfigGetGameID();

	// Set up the Memory Card context.
	CARD_ctx *const ctx = &memCard[slot];
	Init_CARD_ctx(ctx);
	memcpy(ctx->filename, "/saves/", 7);
	char *fname_ptr = &ctx->filename[7];
	if (ConfigGetConfig(NIN_CFG_MC_MULTI))
	{
		// "Multi" mode enabled. (one card for all saves, per region)
		memcpy(fname_ptr, "ninmem", 6);
		fname_ptr += 6;

		if (BI2region == BI2_REGION_JAPAN ||
		    BI2region == BI2_REGION_SOUTH_KOREA)
		{
			// JPN game. Append a 'j'.
			*fname_ptr++ = 'j';
		}

		if (slot)
		{
			// Slot B. Append a 'b'.
			*fname_ptr++ = 'b';
		}

		// Append the file extension. (with NULL terminator)
		memcpy(fname_ptr, ".raw", 5);
	}
	else
	{
		// Single mode. One card per game.
		memcpy(fname_ptr, &GameID, 4);
		fname_ptr += 4;

		if (slot)
		{
			// Slot B. Append "_B".
			*(fname_ptr+0) = '_';
			*(fname_ptr+1) = 'B';
			fname_ptr += 2;
		}

		// Append the file extension. (with NULL terminator)
		memcpy(fname_ptr, ".raw", 5);
	}

	sync_after_write(ctx->filename, sizeof(ctx->filename));

	dbgprintf("EXI: Trying to open %s\r\n", ctx->filename);
	FIL fd;
	int ret = f_open_char(&fd, ctx->filename, FA_READ|FA_OPEN_EXISTING);
	if (ret != FR_OK || fd.obj.objsize == 0)
	{
#ifdef DEBUG_EXI
		dbgprintf("EXI: Slot %c: Failed to open %s: %u\r\n", (slot+'A'), ctx->filename, ret );
#endif
		if (slot == 0)
		{
			// Slot A failure is fatal.
			Shutdown();
		}

		// Slot B failure will simply disable Slot B.
		dbgprintf("EXI: Slot %c has been disabled.\r\n", (slot+'A'));
		return -3;
	}

#ifdef DEBUG_EXI
	dbgprintf("EXI: Loading memory card for Slot %c...", (slot+'A'));
#endif

	// Check if the card filesize is valid.
	u32 FindBlocks = 0;
	for (FindBlocks = 0; FindBlocks <= MEM_CARD_MAX; FindBlocks++)
	{
		if (MEM_CARD_SIZE(FindBlocks) == fd.obj.objsize)
			break;
	}
	if (FindBlocks > MEM_CARD_MAX)
	{
		dbgprintf("EXI: Slot %c unexpected size %s: %u\r\n",
			  (slot+'A'), ctx->filename, fd.obj.objsize);
		if (slot == 0)
		{
			// Slot A failure is fatal.
			Shutdown();
		}

		// Slot B failure will simply disable Slot B.
		dbgprintf("EXI: Slot %c has been disabled.\r\n", (slot+'A'));
		f_close(&fd);
		return -4;
	}

	if (slot == 0)
	{
		// Slot A starts at CARD_base.
		ctx->base = CARD_base;
		// Set the memory card size for Slot A only.
		ConfigSetMemcardBlocks(FindBlocks);
	}
	else
	{
		// Slot B starts immediately after Slot A.
		// Make sure both cards fit within 16 MB.
		if (memCard[0].size + fd.obj.objsize > (16*1024*1024))
		{
			// Not enough memory for both cards.
			// Disable Slot B.
			dbgprintf("EXI: Slot A is %u MB; not enough space for Slot %c, which is %u MB.\r\n",
				  "EXI: Slot %c has been disabled.\r\n",
				  memCard[0].size / 1024 / 1024, (slot+'A'),
				  fd.obj.objsize / 1024 / 1024, (slot+'A'));
			f_close(&fd);
			return -4;
		}
		ctx->base = CARD_base + memCard[0].size;
	}

	// Size and "code".
	ctx->size = fd.obj.objsize;
	ctx->code = MEM_CARD_CODE(FindBlocks);

	// Read the memory card contents into RAM.
	UINT read;
	f_lseek(&fd, 0);
	f_read(&fd, ctx->base, ctx->size, &read);
	f_close(&fd);

	// Reset the low/high offsets to indicate that everything was just loaded.
	ctx->BlockOffLow = 0xFFFFFFFF;
	ctx->BlockOffHigh = 0x00000000;

#ifdef DEBUG_EXI
	dbgprintf("EXI: Loaded Slot %c memory card size %u\r\n", (slot+'A'), ctx->size);
#endif

	// Synchronize the memory card data.
	sync_after_write(ctx->base, ctx->size);

	if (slot == 1)
	{
		// Slot B card image loaded successfully.
		ncfg->Config |= NIN_CFG_MC_SLOTB;
	}

	return 0;
}

void EXIInit(void)
{
	dbgprintf("EXIInit Start\r\n");

	//some important memory for triforce
	ambbBackupMem = malloca(0x10000, 0x40);
	memset32(ambbBackupMem, 0xFF, 0x10000);

	memset32((void*)EXI_BASE, 0, 0x20);
	sync_after_write((void*)EXI_BASE, 0x20);

	// Initialize SRAM.
	SRAM_Init();

	// Clear the "Slot B" configuration bit initially.
	ncfg->Config &= ~NIN_CFG_MC_SLOTB;

	// EXI has been initialized.
	exi_inited = true;

	// If memory card emulation is enabled, load the card images.
	if (ConfigGetConfig(NIN_CFG_MEMCARDEMU))
	{
		// Load Slot A.
		EXILoadCard(0);

		// Load Slot B.
		if (TRIGame == 0)
			EXILoadCard(1);
	}
}

extern vu32 TRIGame;

/**
 * Set EXI timings based on the game ID.
 * @param TitleID Game ID, rshifted by 8.
 * @param Region Region byte from Game ID.
 */
void EXISetTimings(u32 TitleID, u32 Region)
{
	//GC BIOS, Trifoce Game, X-Men Legends 2, Rainbow Six 3 or Starfox Assault (NTSC-U)
	if(useipl || TRIGame != TRI_NONE || TitleID == 0x475832 || TitleID == 0x473633 ||
		(TitleID == 0x474637 && Region == REGION_ID_USA))
	{
		CurrentTiming = EXI_IRQ_INSTANT;
	}
	else if(TitleID == 0x474C4D) //Luigis Mansion (stabilize)
	{
		CurrentTiming = EXI_IRQ_SLOW;
	}
	else
	{
		CurrentTiming = EXI_IRQ_DEFAULT;
	}
#ifdef DEBUG_PATCH
	dbgprintf("Patch:Using a EXI Timing of %i\n", CurrentTiming);
#endif
}

bool EXICheckTimer(void)
{
	return TimerDiffTicks(IRQ_Timer) > CurrentTiming;
}
void EXIInterrupt(void)
{
	write32( 0x10, IRQ_Cause );
	write32( 0x18, IRQ_Cause2 );
	sync_after_write( (void*)0, 0x20 );
	write32( EXI_INT, 0x10 ); // EXI IRQ
	sync_after_write( (void*)EXI_INT, 0x20 );
	write32( HW_IPC_ARMCTRL, 8 ); //throw irq
	//dbgprintf("EXI Interrupt\r\n");
	EXI_IRQ = false;
	IRQ_Timer = 0;
	IRQ_Cause = 0;
	IRQ_Cause2 = 0;
}

/**
 * Get the total size of the loaded memory cards.
 * @return Total size, in bytes.
 */
u32 EXIGetTotalCardSize(void)
{
	return (memCard[0].size + memCard[1].size);
}

/**
 * Check if the memory cards have changed.
 * @return True if either memory card has changed; false if not.
 */
bool EXICheckCard(void)
{
	int slot;
	bool ret = false;
	for (slot = 0; slot < 2; slot++)
	{
		if (memCard[slot].changed)
		{
			memCard[slot].changed = false;
			ret = true;
		}
	}
	return ret;
}

/**
 * Save the memory card(s).
 */
void EXISaveCard(void)
{
	if (TRIGame)
	{
		// Triforce doesn't use the standard EXI CARD interface.
		return;
	}

	int slot;
	for (slot = 0; slot < 2; slot++)
	{
		if (!isCardEnabled(slot))
		{
			// Card isn't initialized.
			continue;
		}

		// Does this card have any unsaved changes?
		CARD_ctx *const ctx = &memCard[slot];
		if (ctx->BlockOffLow < ctx->BlockOffHigh)
		{
//#ifdef DEBUG_EXI
			//dbgprintf("EXI: Saving memory card in Slot %c...", (slot+'A'));
//#endif
			FIL fd;
			int ret = f_open_char(&fd, ctx->filename, FA_WRITE|FA_OPEN_EXISTING);
			if (ret == FR_OK)
			{
				UINT wrote;
				sync_before_read(ctx->base, ctx->size);
				f_lseek(&fd, ctx->BlockOffLow);
				f_write(&fd, &ctx->base[ctx->BlockOffLow],
					(ctx->BlockOffHigh - ctx->BlockOffLow), &wrote);
				f_close(&fd);
//#ifdef DEBUG_EXI
				//dbgprintf("Done!\r\n");
//#endif
			}
			else
			{
				dbgprintf("\r\nEXI: Unable to open Slot %c memory card file: %u\r\n", (slot+'A'), ret);
			}
			// Reset the low/high offsets to indicate that everything has been saved.
			ctx->BlockOffLow = 0xFFFFFFFF;
			ctx->BlockOffHigh = 0x00000000;
		}
	}
}

void EXIShutdown(void)
{
	if (TRIGame || !exi_inited)
	{
		// Triforce doesn't use the standard EXI CARD interface.
		return;
	}

//#ifdef DEBUG_EXI
	dbgprintf("EXI: Saving memory card(s)...");
//#endif
	EXISaveCard();
//#ifdef DEBUG_EXI
	dbgprintf("Done!\r\n");
//#endif
	exi_inited = false;
}

/**
 * EXI device handler: Memory Card
 * @param slot Memory card slot. (0 == Slot A, 1 == Slot B)
 * @param Data Data buffer.
 * @param Length Length of data buffer.
 * @param Mode Read/Write mode.
 */
static void EXIDeviceMemoryCard(int slot, u8 *Data, u32 Length, u32 Mode)
{
	if (!isCardEnabled(slot))
	{
		// Card is not enabled.
		return;
	}

	CARD_ctx *const ctx = &memCard[slot];
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
						dbgprintf("EXI: Slot %c: CARDGetDeviceIDNintendo()\r\n", (slot+'A'));
#endif
					} break;
#ifdef DEBUG_EXI					
					case 0x89:
					{
						dbgprintf("EXI: Slot %c: CARDClearStatus()\r\n", (slot+'A'));
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
						dbgprintf("EXI: Slot %c: CARDGetDeviceID()\r\n", (slot+'A'));
#endif
					} break;
					case 0x8300:	//
					{
						EXICommand = MEM_READ_STATUS;
#ifdef DEBUG_EXI
						dbgprintf("EXI: Slot %c: CARDReadStatus()\r\n", (slot+'A'));
#endif
					} break;
#ifdef DEBUG_EXI
					case 0x8101:
					{
						dbgprintf("EXI: Slot %c: CARDIRQEnable()\r\n", (slot+'A'));
					} break;
					case 0x8100:
					{
						dbgprintf("EXI: Slot %c: CARDIRQDisable()\r\n", (slot+'A'));
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
						ctx->BlockOff = (((u32)Data>>16)&0xFF)  << 17;
						ctx->BlockOff|= (((u32)Data>> 8)&0xFF)  << 9;
#ifdef DEBUG_EXI
						dbgprintf("EXI: Slot %c: CARDErasePage(%08X)\r\n", (slot+'A'), ctx->BlockOff);
#endif
						EXICommand = MEM_BLOCK_ERASE;
						ctx->CARDWriteCount = 0;
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
						ctx->BlockOff = (((u32)Data>>16)&0xFF)  << 17;
						ctx->BlockOff|= (((u32)Data>> 8)&0xFF)  << 9;
						ctx->BlockOff|= (((u32)Data&0xFF) & 3 ) << 7;
#ifdef DEBUG_EXI
						dbgprintf("EXI: Slot %c: CARDErasePage(%08X)\r\n", (slot+'A'), ctx->BlockOff);
#endif
						EXICommand = MEM_BLOCK_ERASE;
						ctx->CARDWriteCount = 0;
						IRQ_Cause = 2;			// EXI IRQ
						EXIOK = 2;
					} break;
					case 0xF2:
					{
						ctx->BlockOff = (((u32)Data>>16)&0xFF)  << 17;
						ctx->BlockOff|= (((u32)Data>> 8)&0xFF)  << 9;
						ctx->BlockOff|= (((u32)Data&0xFF) & 3 ) << 7;
#ifdef DEBUG_EXI
						dbgprintf("EXI: Slot %c: CARDWritePage(%08X)\r\n", (slot+'A'), ctx->BlockOff);
#endif
						EXICommand = MEM_BLOCK_WRITE;
					} break;
					case 0x52:
					{
						ctx->BlockOff = (((u32)Data>>16)&0xFF)  << 17;
						ctx->BlockOff|= (((u32)Data>> 8)&0xFF)  << 9;
						ctx->BlockOff|= (((u32)Data&0xFF) & 3 ) << 7;
#ifdef DEBUG_EXI
						dbgprintf("EXI: Slot %c: CARDReadPage(%08X)\r\n", (slot+'A'), ctx->BlockOff);
#endif

						EXICommand = MEM_BLOCK_READ;
					} break;
#ifdef DEBUG_EXI
					default:
					{
						dbgprintf("EXI: Slot %c: Unknown:%08x Line:%u\r\n", (slot+'A'), (u32)Data, __LINE__ );
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
						// Update the block offsets for saving.
						if (ctx->BlockOff < ctx->BlockOffLow)
							ctx->BlockOffLow = ctx->BlockOff;
						if (ctx->BlockOff + Length > ctx->BlockOffHigh)
							ctx->BlockOffHigh = ctx->BlockOff + Length;
						ctx->changed = true;

						// FIXME: Verify that this doesn't go out of bounds.
						sync_before_read(Data, Length);
						memcpy(&ctx->base[ctx->BlockOff], Data, Length);
						sync_after_write(&ctx->base[ctx->BlockOff], Length);

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
				if( ConfigGetConfig(NIN_CFG_MEMCARDEMU) && (TRIGame == TRI_NONE) )
				{
					write32( EXI_CMD_1, ctx->code );
				} else {
					write32( EXI_CMD_1, 0x00000000 ); //no memory card
				}
#ifdef DEBUG_EXI
				dbgprintf("EXI: Slot %c: CARDReadID(%X)\r\n", (slot+'A'), read32(EXI_CMD_1));
#endif
			} break;
			case MEM_READ_STATUS:
			{
				write32( EXI_CMD_1, 0x41 );	// Unlocked(0x40) and Ready(0x01)
#ifdef DEBUG_EXI
				dbgprintf("EXI: Slot %c: CARDReadStatus(%X)\r\n", (slot+'A'), read32(EXI_CMD_1));
#endif
			} break;
			case MEM_BLOCK_READ:
			{
				//f_lseek( &MemCard, BlockOff );
				//f_read( &MemCard, Data, Length, &read );
				sync_before_read(&ctx->base[ctx->BlockOff], Length);
				memcpy(Data, &ctx->base[ctx->BlockOff], Length);
				sync_after_write(Data, Length);

				IRQ_Cause = 8;		// TC IRQ

				EXIOK = 2;
			} break;
		}
	}

	//dbgprintf("%08x %08x %08x %08x\r\n", (u32)Data >> 16, Mode, Length, EXICommand);
	write32( EXI_CMD_0, 0 ); //exit EXIDMA / EXIImm
	sync_after_write( (void*)EXI_BASE, 0x20 );

	if( EXIOK == 2 )
	{
		EXI_IRQ = true;
		IRQ_Timer = read32(HW_TIMER);
	}
}

static u32 EXIDevice_ROM_RTC_SRAM_UART(u8 *Data, u32 Length, u32 Mode)
{
	if( Mode == 1 )		// Write
	{
		switch( Length )
		{
			case 4:
			{
				if( EXICommand == SRAM_WRITE )
				{
					*(u32*)(((u8*)&sram)+SRAMWriteCount) = (u32)Data;

					SRAMWriteCount += Length;

					break;
				}

				if( (u32)Data == 0x20000000 )
				{
					EXICommand = RTC_READ;
#ifdef DEBUG_SRAM
					dbgprintf("EXI: RTCRead()\r\n");
#endif
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

				if( (u32)Data == 0x20010000 )
				{
					EXICommand = UART_READ;
#ifdef DEBUG_SRAM
					dbgprintf("EXI: UARTRead()\r\n");
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
				u32 PossibleIPLOffset = (u32)Data >> 6;
				if( PossibleIPLOffset >= IPL_ROM_FONT_SJIS && PossibleIPLOffset < IPL_ROM_END_OFFSET )
				{
					EXICommand = IPL_READ_FONT;
#ifdef DEBUG_SRAM
					dbgprintf("EXI: IPLReadFont()\r\n");
#endif
					IPLReadOffset = PossibleIPLOffset;
					break;
				}

			} break;
		}

	} else {

		switch( EXICommand )
		{
			case IPL_READ_FONT:
			{
//#ifdef DEBUG_SRAM
				dbgprintf("EXI: IPLRead( %p, %08X, %u), No ReadROM Patch?\r\n", Data, IPLReadOffset, Length );
//#endif
				EXIReadFontFile(Data, Length);
			} break;
			case RTC_READ:
			{
				Data = (u8*)P2C((u32)Data); //too small size so do it manually
#ifdef DEBUG_SRAM
				dbgprintf("EXI: RTCRead( %p, %u)\r\n", Data, Length );
#endif			//???
				sync_before_read_align32( Data, Length );
				memset( Data, 0, Length );
				sync_after_write_align32( Data, Length );
			} break;
			case SRAM_READ:
			{
#ifdef DEBUG_SRAM
				dbgprintf("EXI: SRAMRead(%p,%u)\r\n", Data, Length );
#endif
				memcpy( Data, &sram, Length );
				sync_after_write( Data, Length );
			} break;
			case UART_READ:
			{
				Data = (u8*)P2C((u32)Data); //too small size so do it manually
#ifdef DEBUG_SRAM
				dbgprintf("EXI: UARTRead( %p, %u)\r\n", Data, Length );
#endif			//???
				sync_before_read_align32( Data, Length );
				memset( Data, 0, Length );
				sync_after_write_align32( Data, Length );
			} break;
			default:
			{
				dbgprintf("EXI: Unknown:%08x Line:%u\r\n", (u32)Data, __LINE__ );
				Shutdown();
			} break;
		}
	}

	write32( EXI_CMD_0, 0 );
	sync_after_write( (void*)EXI_BASE, 0x20 );
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
				// FIXME: Use CARD_ctx for Triforce?
				switch( (u32)Data >> 24 )
				{
					case 0x01:
					{
						TRIBackupOffset = ((u32)Data >> 8) & 0xFFFF;
						EXICommand = AMBB_BACKUP_OFFSET;
					} break;
					case 0x02:
					{
						ambbBackupMem[TRIBackupOffset] = ((u32)Data >> 16) & 0xFF;
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
				write32( EXI_CMD_1, 0x01 );
			} break;
			/* 0x03 */
			case AMBB_BACKUP_READ:
			{
				write32( EXI_CMD_1, 0x0100 | ambbBackupMem[TRIBackupOffset] );
			} break;
			/* 0x87 */
			case AMBB_IMR_WRITE:
			/* 0x83 */
			case AMBB_UNKNOWN:
			{
				write32( EXI_CMD_1, 0x04 );
			} break;
			/* 0x82 */
			case AMBB_ISR_READ:
			{
				if( Length == 1 )
				{
					write32( EXI_CMD_1, 0x04 );
				} else {
					write32( EXI_CMD_1, EXI2IRQStatus );
					EXI2IRQ = 0;
				}
			} break;
			/* 0x86 */
			case AMBB_IMR_READ:
			{
				if( Length == 1 )
				{
					write32( EXI_CMD_1, 0x04 );
				} else {
					write32( EXI_CMD_1, 0x0000 );
				}
			} break;
			/* 0xFF */
			case AMBB_LANCNT_WRITE:
			{
				if( EXI2IRQ )
				{
					EXIOK = 2;
				}
				write32( EXI_CMD_1, 0x04 );
			} break;
			case MEM_READ_ID:
			{
				write32( EXI_CMD_1, EXI_DEVTYPE_BASEBOARD );
			} break;
		}
	}

	write32( EXI_CMD_0, 0 );
	sync_after_write((void*)EXI_BASE, 0x20);

	if( EXIOK == 2 )
	{
		//dbgprintf("EXI: Triforce IRQ\r\n");
		IRQ_Cause = 8;
		IRQ_Cause2 = 2;
		EXI_IRQ = true;
		IRQ_Timer = read32(HW_TIMER);
	}
	return 0;
}

void EXIUpdateRegistersNEW( void )
{
	if( EXI_IRQ == true ) //still working
		return;

	//u32 chn, dev, frq, ret, data, len, mode;
	u32 chn, dev, ret, data, len, mode;
	u8 *ptr;
	sync_before_read((void*)EXI_BASE, 0x20);
	u32 command = read32(EXI_CMD_0);

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

				ret = 0;
				if (chn <= 2 && dev <= 2)
				{
					// Valid channel/device number.
					EXIDeviceSelect[chn] = dev;

					// Check if this device is present.
					switch (EXI_DEVICE_NUMBER(chn, dev))
					{
						case EXI_DEV_MEMCARD_A:
						case EXI_DEV_MASK_ROM_RTC_SRAM_UART:
						case EXI_DEV_SP1:
							// Device is present.
							// TODO: SP1 only for Triforce?
							ret = 1;
							break;

						case EXI_DEV_MEMCARD_B:
							ret = isCardEnabled(1);
							break;

						case EXI_DEV_AD16:
						default:
							// Unsupported device.
							ret = 0;
							break;
					}
				}

				EXICommand = 0;
				write32( EXI_CMD_1, ret );
				write32( EXI_CMD_0, 0 );
				sync_after_write((void*)EXI_BASE, 0x20);
			} break;
			case 0x11:	// EXI_Imm( s32 nChn, void *pData, u32 nLen, u32 nMode, EXICallback tc_cb );
			{
				chn	= (command >> 20) & 0xF;
				data	= read32(EXI_CMD_1);
				len	= command& 0xFFFF;
				mode	= (command >> 16) & 0xF;

				if( len > 4 )
				{
					data = P2C(data);
				}
				
				//dbgprintf("EXIImm( %u, %p, %u, %u, Dev:%u EC:%u )\r\n", chn, data, len, mode, Device, EXICommand );
				switch (EXI_DEVICE_NUMBER(chn, EXIDeviceSelect[chn&3]))
				{
					case EXI_DEV_MEMCARD_A:
						EXIDeviceMemoryCard(0, (u8*)data, len, mode);
						break;

					case EXI_DEV_MEMCARD_B:
						EXIDeviceMemoryCard(1, (u8*)data, len, mode);
						break;

					case EXI_DEV_MASK_ROM_RTC_SRAM_UART:
						EXIDevice_ROM_RTC_SRAM_UART( (u8*)data, len, mode );
						break;

					case EXI_DEV_SP1:
						EXIDeviceSP1( (u8*)data, len, mode );
						break;

					default:
#ifdef DEBUG_SRAM
						dbgprintf("EXI: EXIImm: Unhandled device: Ch%u, Dev%u\r\n", chn, EXIDeviceSelect[chn&3]);
#endif
						break;
				}

			} break;
			case 0x12:	// EXIDMA
			{
				chn	= (command >> 20) & 0xF;
				ptr	= (u8*)P2C(read32(EXI_CMD_1));
				len	= command& 0xFFFF;
				mode	= (command >> 16) & 0xF;
				
				//dbgprintf("EXIDMA( %u, %p, %u, %u )\r\n", chn, ptr, len, mode );
				switch (EXI_DEVICE_NUMBER(chn, EXIDeviceSelect[chn&3]))
				{
					case EXI_DEV_MEMCARD_A:
						EXIDeviceMemoryCard(0, ptr, len, mode);
						break;

					case EXI_DEV_MEMCARD_B:
						EXIDeviceMemoryCard(1, ptr, len, mode);
						break;

					case EXI_DEV_MASK_ROM_RTC_SRAM_UART:
						EXIDevice_ROM_RTC_SRAM_UART( ptr, len, mode );
						break;

					case EXI_DEV_SP1:
#ifdef DEBUG_SRAM
						hexdump( ptr, len );
#endif
						EXIDeviceSP1( ptr, len, mode );
						break;

					default:
#ifdef DEBUG_SRAM
						dbgprintf("EXI: EXIDMA: Unhandled device: Ch%u, Dev%u\r\n", chn, EXIDeviceSelect[chn&3]);
#endif
						break;
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

//SegaBoot 3.11 with Free Play enabled
static const unsigned int sb311block[54] =
{
    0x41434255, 0x30303031, 0x007D0512, 0x01000000, 0x00000311, 0x53424C4B, 
    0x00000000, 0x63090400, 0x01010A01, 0x01010001, 0x01010101, 0x01010101, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000001, 
    0x00000000, 0x00000000, 0x00000000, 0x00000001, 0x00000000, 0x00000000, 
    0x00000000, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x00000001, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x200E1AFF,
    0xFFFF0000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

extern u32 arcadeMode;
static bool TRIGameStarted = false;
//make sure ambbBackupMem is filled correctly
void EXIPrepareTRIGameStart()
{
	if(TRIGameStarted)
		return;
	dbgprintf("TRI:Setting up AMBB memory\r\n");
	memset32(ambbBackupMem, 0, 0x400);
	memcpy(ambbBackupMem, sb311block, sizeof(sb311block));
	memcpy(ambbBackupMem + 0x200, sb311block, sizeof(sb311block));
	if(arcadeMode)
	{	//standard coin settings instead of free play
		ambbBackupMem[0x00B] = 8; ambbBackupMem[0x022] = 1; ambbBackupMem[0x023] = 0;
		ambbBackupMem[0x20B] = 8; ambbBackupMem[0x222] = 1; ambbBackupMem[0x223] = 0;
	}
	memset32(ambbBackupMem + 0x400, 0xFF, 0x10000 - 0x400);
	TRIGameStarted = true;
}
