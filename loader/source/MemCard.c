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

/* Same Method as SRAM Checksum in Kernel */
static void doChecksum(u16 *buffer, u32 size, u16 *c1, u16 *c2)
{
	u32 i;
	u32 len = size / sizeof(u16);
	*c1 = 0; *c2 = 0;
	for(i = 0; i < len; ++i)
	{
		*c1 += buffer[i];
		*c2 += buffer[i] ^ 0xFFFF;
	}
	if(*c1 == 0xFFFF) *c1 = 0;
	if(*c2 == 0xFFFF) *c2 = 0;
}

bool GenerateMemCard(char *MemCard)
{
	if(MemCard == NULL)
		return false;
	FILE *f = fopen(MemCard, "wb");
	if(f == NULL)
		return false;
	//Get memory to format
	u8 *MemcardBase = malloc(MEM_CARD_SIZE(ncfg->MemCardBlocks));
	memset(MemcardBase, 0, MEM_CARD_SIZE(ncfg->MemCardBlocks));
	//Fill Header and Dir Memory with 0xFF
	memset(MemcardBase, 0xFF, 0x6000);
	//Normally this area contains a checksum which is built via SRAM and formatting time, you can
	//skip that though because if the formatting time is 0 the resulting checksum is also 0 ;)
	memset(MemcardBase, 0, 0x14);
	//From SRAM as defined in PatchCodes.h
	u32 TmpU32 = 0x17CA2A85;
	memcpy(MemcardBase + 0x14, &TmpU32, 4);
	//Use current language for SRAM language
	TmpU32 = ncfg->Language;
	memcpy(MemcardBase + 0x18, &TmpU32, 4);
	//Not sure what this area is about
	memset(MemcardBase + 0x1C, 0, 6);
	//Memory Card size in MBits total
	u16 TmpShort = MEM_CARD_SIZE(ncfg->MemCardBlocks) >> 17;
	memcpy(MemcardBase + 0x22, &TmpShort, 2);
	//Memory Card File Mode
	TmpShort = (ncfg->GameID & 0xFF) == 'J';
	memcpy(MemcardBase + 0x24, &TmpShort, 2);
	//Generate Header Checksum
	doChecksum((u16*)(MemcardBase),0x1FC,(u16*)(MemcardBase+0x1FC),(u16*)(MemcardBase+0x1FE));
	//Set Dir Update Counters
	TmpShort = 0;
	memcpy(MemcardBase + 0x3FFA, &TmpShort, 2);
	TmpShort = 1;
	memcpy(MemcardBase + 0x5FFA, &TmpShort, 2);
	//Generate Dir Checksums
	doChecksum((u16*)(MemcardBase+0x2000),0x1FFC,(u16*)(MemcardBase+0x3FFC),(u16*)(MemcardBase+0x3FFE));
	doChecksum((u16*)(MemcardBase+0x4000),0x1FFC,(u16*)(MemcardBase+0x5FFC),(u16*)(MemcardBase+0x5FFE));
	//Set Block Update Counters
	TmpShort = 0;
	memcpy(MemcardBase + 0x6004, &TmpShort, 2);
	TmpShort = 1;
	memcpy(MemcardBase + 0x8004, &TmpShort, 2);
	//Set Free Blocks
	TmpShort = MEM_CARD_BLOCKS(ncfg->MemCardBlocks);
	memcpy(MemcardBase + 0x6006, &TmpShort, 2);
	memcpy(MemcardBase + 0x8006, &TmpShort, 2);
	//Set last allocated Block
	TmpShort = 4;
	memcpy(MemcardBase + 0x6008, &TmpShort, 2);
	memcpy(MemcardBase + 0x8008, &TmpShort, 2);
	//Generate Block Checksums
	doChecksum((u16*)(MemcardBase+0x6004),0x1FFC,(u16*)(MemcardBase+0x6000),(u16*)(MemcardBase+0x6002));
	doChecksum((u16*)(MemcardBase+0x8004),0x1FFC,(u16*)(MemcardBase+0x8000),(u16*)(MemcardBase+0x8002));
	//Write it into a file
	fwrite(MemcardBase, 1, MEM_CARD_SIZE(ncfg->MemCardBlocks), f);
	fclose(f);
	free(MemcardBase);
	gprintf("Memory Card File created!\r\n");
	return true;
}
