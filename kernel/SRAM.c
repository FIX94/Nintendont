// Nintendont: SRAM functions.
#include "SRAM.h"
#include "Config.h"

extern u32 Region;

// Initial SRAM values.
GC_SRAM sram ALIGNED(32) =
{
	0x428B,		// Checksum 1
	0xBD71,		// Checksum 2
	0x00000000,	// ead0
	0x00000000,	// ead1
	0x17CA2A85,	// RTC counter bias
	0x00,		// Display offset H
	0x00,		// Boot mode (Bit 6 = PAL60 flag)
	0x00,		// Language
	0x3C,		// Flags

	// Flash IDs
	{{0x9B, 0x58, 0x5A, 0xB5, 0xB6, 0xC7, 0x92, 0xB7, 0x55, 0x49, 0xC6, 0x0B},
	 {0x4A, 0x09, 0x00, 0x45, 0x0D, 0x00, 0xB2, 0x1D, 0x41, 0x03, 0x88, 0x1D}},

	0x49415004,	// Wireless keyboard ID

	// Wireless pad IDs
	{0x0000, 0x0000, 0x0000, 0x0000},

	0xFE,		// Last DVD error
	0x00,		// Reserved
	{0xC8, 0xC8},	// Flash ID checksums

	// Unused
	0x01055728
};

/**
 * Update the SRAM checksum.
 */
void SRAM_UpdateChecksum(void)
{
	// Temporary checksums.
	// NOTE: Only the low 16 bits are significant.
	u32 chk = 0, chkinv = 0;

	// Checksummed data starts at the RTC counter bias. (0x0C)
	const u16 *sram_data = (u16*)(&sram.CounterBias);

	// Processing in u16 increments, so divide the
	// actual addresses by two.
	u32 i;
	for (i = (0x0C>>1); i < (0x14>>1); i++, sram_data++)
	{
		chk += *sram_data;
		chkinv += (*sram_data ^ 0xFFFF);
	}

	sram.CheckSum1 = (chk & 0xFFFF);
	sram.CheckSum2 = (chkinv & 0xFFFF);
	//dbgprintf("New Checksum: %04X %04X\r\n", chk, chkinv);
}

/**
 * Initialize SRAM.
 */
void SRAM_Init(void)
{
	// Clear the Flash IDs.
	memset(sram.FlashID, 0, sizeof(sram.FlashID));
	sram.FlashIDChecksum[0] = 0xFF;

	sram.BootMode &= ~0x40;	// Clear PAL60
	sram.Flags    &= ~0x80;	// Clear Progmode
	sram.Flags    &= ~3;	// Clear Videomode
	sram.DisplayOffsetH = 0;

	const u32 GameID = ConfigGetGameID();
	switch (GameID & 0xFF)
	{
		case 'E':
		case 'J':
			//BMX XXX doesnt even boot on a real gc with component cables
			if ((ConfigGetGameID() >> 8) != 0x474233 &&
			    (ConfigGetVideoMode() & NIN_VID_PROG))
			{
				sram.Flags |= 0x80;	// Set Progmode
			}
			break;
		default:
			sram.BootMode	|= 0x40;	// Set PAL60
			break;
	}

	sram.Language = ConfigGetLanguage();

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
			sram.Flags		|= 1;

		} break;
	}

	// Update the SRAM checksum.
	SRAM_UpdateChecksum();
}

/**
 * Set the flash ID of a memory card in SRAM.
 * @param base Memory card base address.
 * @param slot Slot number. (0 or 1)
 */
void SRAM_SetFlashID(const u8 *base, int slot)
{
	// Set the memory card's flash ID in SRAM.
	// References:
	// - https://github.com/dolphin-emu/dolphin/blob/3ff56aa192f952cb6ad2e9c662d4a368efd21e6f/Source/Core/Core/HW/Sram.cpp#L79
	// - https://github.com/dolphin-emu/dolphin/blob/3ff56aa192f952cb6ad2e9c662d4a368efd21e6f/Source/Core/Core/HW/EXI_DeviceMemoryCard.cpp#L141
	// - https://github.com/suloku/gcmm/blob/75a5e024b7f590d390e1c74fe1524dbfc2bd6b99/source/raw.c

	// Make sure the slot number is valid.
	if (slot != 0 && slot != 1)
		return;

	u64 rand;	// Random number state.
	u8 csum = 0;	// Flash ID checksum.
	int i;

	// Initialize the random number state from the card's format time.
	memcpy(&rand, &base[12], sizeof(rand));
	u8 *flash_id = sram.FlashID[slot];

	// This is a standard Linear Congruential Generator.
	// The parameters match the original K&R C rand() specification.
	// https://en.wikipedia.org/wiki/Linear_congruential_generator
	// https://sourceware.org/git/?p=glibc.git;a=blob;f=stdlib/random_r.c
	// http://wiki.osdev.org/Random_Number_Generator
	for (i = 0; i < 12; i++)
	{
		rand = (((rand * 1103515245ULL) + 12345ULL) >> 16);
		flash_id[i] = base[i] - ((u8)rand & 0xFF);
		csum += flash_id[i];
		rand = (((rand * 1103515245ULL) + 12345ULL) >> 16);
		rand &= 0x7FFFULL;
	}

	// Store the flash ID checksum.
	sram.FlashIDChecksum[slot] = csum;

	// Update the SRAM checksum.
	SRAM_UpdateChecksum();
}
