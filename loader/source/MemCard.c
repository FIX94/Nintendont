/*
MemCard.c for Nintendont (Kernel)

Copyright (C) 2014 FIX94

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
#include "ff_utf8.h"
#include "menu.h"
#include "font.h"

// Memory card header.
typedef struct __attribute__ ((packed)) _card_header
{
	u8 reserved1[12];
	u64 formatTime;		// Format time. (OSTime value; 1 tick == 1/40,500,000 sec)
	u32 sramBias;		// SRAM bias at time of format.
	u32 sramLang;		// SRAM language.
	u32 reserved2;

	u16 device_id;		// 0 if formatted in slot A; 1 if formatted in slot B
	u16 size;		// size of card, in Mbits
	u16 encoding;		// 0 == cp1252; 1 == Shift-JIS

	u8 reserved3[0x1D6];	// Should be 0xFF.
	u16 chksum1;		// Checksum.
	u16 chksum2;		// Inverted checksum.
} card_header;


/* Same Method as SRAM Checksum in Kernel */
static void doChecksum(const u16 *buffer, u32 size, u16 *c1, u16 *c2)
{
	*c1 = 0; *c2 = 0;
	u32 i;
	for (i = size/sizeof(u16); i > 0; i--, buffer++)
	{
		*c1 += *buffer;
		*c2 += *buffer ^ 0xFFFF;
	}
	if (*c1 == 0xFFFF) *c1 = 0;
	if (*c2 == 0xFFFF) *c2 = 0;
}

/**
 * Show memory card formatting progress.
 * @param written Bytes written.
 * @param total Total bytes to write.
 */
static void showProgress(unsigned int written, unsigned int total)
{
	ClearScreen();
	PrintInfo();

	char buf[128];
	int len, x;

	// First line.
	len = snprintf(buf, sizeof(buf),
		"Initializing virtual %u-block memory card...",
		MEM_CARD_BLOCKS(ncfg->MemCardBlocks));
	x = (640 - (len*10)) / 2;
	PrintFormat(DEFAULT_SIZE, BLACK, x, 232-20, "%s", buf);

	// Second line.
	len = snprintf(buf, sizeof(buf),
		"%u of %u KiB written",
		written / 1024, total / 1024);
	x = (640 - (len*10)) / 2;
	PrintFormat(DEFAULT_SIZE, BLACK, x, 232+20, "%s", buf);

	// Render the text.
	GRRLIB_Render();
	ClearScreen();
}

/**
 * Create a blank memory card image.
 * @param MemCard Memory card filename.
 * @param BI2region bi2.bin region code.
 * @return True on success; false on error.
 */
bool GenerateMemCard(const char *MemCard, u32 BI2region)
{
	if (!MemCard || MemCard[0] == 0)
		return false;

	FIL f;
	if (f_open_char(&f, MemCard, FA_WRITE|FA_CREATE_NEW) != FR_OK)
		return false;

	// Get memory to format. (8 block window)
	u8 *MemcardBase = memalign(32, 0x10000);
	// Fill Header and Dir Memory with 0xFF.
	memset(MemcardBase, 0xFF, 0x6000);
	// Fill the Block table with 0x00.
	memset(&MemcardBase[0x6000], 0x00, 0x4000);
	// Clear the initial data area.
	memset(&MemcardBase[0xA000], 0x00, 0x6000);

	// Header block.
	card_header *header = (card_header*)MemcardBase;
	//Normally this area contains a checksum which is built via SRAM and formatting time, you can
	//skip that though because if the formatting time is 0 the resulting checksum is also 0 ;)
	memset(header->reserved1, 0, sizeof(header->reserved1));
	header->formatTime = 0;		// TODO: Actually set this?
	//From SRAM as defined in PatchCodes.h
	header->sramBias = 0x17CA2A85;
	//Use current language for SRAM language
	header->sramLang = ncfg->Language;
	//Assuming slot A.
	header->device_id = 0;
	//Memory Card size in MBits total
	header->size = MEM_CARD_SIZE(ncfg->MemCardBlocks) >> 17;

	// Region-specific data.
	switch (BI2region)
	{
		case BI2_REGION_JAPAN:
		case BI2_REGION_SOUTH_KOREA:
			// JPN games.
			header->reserved2 = 2;	// "File mode"?
			header->encoding = 1;	// Encoding. (Shift-JIS)
			break;

		case BI2_REGION_USA:
		case BI2_REGION_PAL:
		default:
			// USA/PAL games.
			header->reserved2 = 0;	// "File mode"?
			header->encoding = 0;	// Encoding. (cp1252)
			break;
	}

	//Generate Header Checksum
	doChecksum((u16*)header, 0x1FC, &header->chksum1, &header->chksum2);

	//Set Dir Update Counters
	*((u16*)&MemcardBase[0x3FFA]) = 0;
	*((u16*)&MemcardBase[0x5FFA]) = 1;
	//Generate Dir Checksums
	doChecksum((u16*)&MemcardBase[0x2000], 0x1FFC, (u16*)&MemcardBase[0x3FFC], (u16*)&MemcardBase[0x3FFE]);
	doChecksum((u16*)&MemcardBase[0x4000], 0x1FFC, (u16*)&MemcardBase[0x5FFC], (u16*)&MemcardBase[0x5FFE]);

	//Set Block Update Counters
	*((u16*)&MemcardBase[0x6004]) = 0;
	*((u16*)&MemcardBase[0x8004]) = 1;
	//Set Free Blocks
	*((u16*)&MemcardBase[0x6006]) = MEM_CARD_BLOCKS(ncfg->MemCardBlocks);
	*((u16*)&MemcardBase[0x8006]) = MEM_CARD_BLOCKS(ncfg->MemCardBlocks);
	//Set last allocated Block
	*((u16*)&MemcardBase[0x6008]) = 4;
	*((u16*)&MemcardBase[0x8008]) = 4;
	//Generate Block Checksums
	doChecksum((u16*)&MemcardBase[0x6004], 0x1FFC, (u16*)&MemcardBase[0x6000], (u16*)&MemcardBase[0x6002]);
	doChecksum((u16*)&MemcardBase[0x8004], 0x1FFC, (u16*)&MemcardBase[0x8000], (u16*)&MemcardBase[0x8002]);

	const u32 total_size = MEM_CARD_SIZE(ncfg->MemCardBlocks);

	// Reserve space in the memory card file.
	f_expand(&f, total_size, 1);

	// Write the header (5 blocks) and initial data area
	// (3 blocks) to the file.
	UINT wrote;
	showProgress(0, total_size);
	f_write(&f, MemcardBase, 0x10000, &wrote);

	// Write the remaining blocks.
	u32 i;
	for (i = 0x10000; i < total_size; i += 0x10000)
	{
		showProgress(i, total_size);
		f_write(&f, MemcardBase, 0x10000, &wrote);
	}
	showProgress(total_size, total_size);

	f_close(&f);
	free(MemcardBase);
	gprintf("Memory Card File created!\r\n");

	// Flush the devices to make sure everything is written.
	FlushDevices();
	return true;
}
