/*
PatchWidescreen.c for Nintendont (Kernel)

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
#include "debug.h"
#include "common.h"
#include "string.h"
#include "PatchWidescreen.h"
#include "asm/CalcWidescreen.h"
#include "asm/CalcWidescreenDiv.h"
#include "asm/CalcGXWidescreen.h"

extern void PatchB(u32 dst, u32 src);
extern u32 PatchCopy(const u8 *PatchPtr, const u32 PatchSize);
void PatchWideMulti(u32 BufferPos, u32 dstReg)
{
	u32 wide = PatchCopy(CalcWidescreen, CalcWidescreen_size);
	/* Copy original instruction and jump */
	write32(wide, read32(BufferPos));
	PatchB(wide, BufferPos);
	/* Modify destination register */
	write32(wide+0x28, (read32(wide+0x28) & 0xFC1FF83F) | (dstReg << 6) | (dstReg << 21));
	/* Jump back to original code */
	PatchB(BufferPos+4, wide+CalcWidescreen_size-4);
}

void PatchWideDiv(u32 BufferPos, u32 dstReg)
{
	u32 wide = PatchCopy(CalcWidescreenDiv, CalcWidescreenDiv_size);
	/* Copy original instruction and jump */
	write32(wide, read32(BufferPos));
	PatchB(wide, BufferPos);
	/* Modify destination register */
	write32(wide+0x28, (read32(wide+0x28) & 0xFC1FF83F) | (dstReg << 6) | (dstReg << 21));
	/* Jump back to original code */
	PatchB(BufferPos+4, wide+CalcWidescreenDiv_size-4);
}

void PatchGXWideMulti(u32 BufferPos, u32 dstReg)
{
	u32 wide = PatchCopy(CalcGXWidescreen, CalcGXWidescreen_size);
	/* Copy original instruction and jump */
	write32(wide+CalcGXWidescreen_size-8, read32(BufferPos));
	PatchB(wide, BufferPos);
	/* Jump back to original code */
	PatchB(BufferPos+4, wide+CalcGXWidescreen_size-4);
}

//Credits to Ralf from gc-forever for the original Ocarina Codes

#define FLT_ASPECT_0_913 0x3f69d89c
#define FLT_ASPECT_1_218 0x3f9be5bd

#define FLT_ASPECT_1_200 0x3f99999a
#define FLT_ASPECT_1_600 0x3fcccccd

#define FLT_ASPECT_1_266 0x3fa22222
#define FLT_ASPECT_1_688 0x3fd82d83

#define FLT_ASPECT_1_333 0x3faaaaab
#define FLT_ASPECT_1_777 0x3fe38e39

#define FLT_ASPECT_1_357 0x3fadb6db
#define FLT_ASPECT_1_809 0x3fe79e7a

#define FLT_ASPECT_1_428 0x3fb6db6e
#define FLT_ASPECT_1_905 0x3ff3cf3d

bool PatchWidescreen(u32 FirstVal, u32 Buffer)
{
	if(FirstVal == FLT_ASPECT_0_913 && read32(Buffer+4) == 0x2e736200)
	{
		write32(Buffer, FLT_ASPECT_1_218);
		dbgprintf("PatchWidescreen:[Aspect Ratio 1.218] applied (0x%08X)\r\n", Buffer );
		return true;
	}
	else if(FirstVal == FLT_ASPECT_1_200 && (read32(Buffer+4) == 0x43F00000 || 
			(read32(Buffer+4) == 0 && read32(Buffer+8) == 0x43F00000)))
	{	//All Mario Party games share this value
		write32(Buffer, FLT_ASPECT_1_600);
		dbgprintf("PatchWidescreen:[Aspect Ratio 1.600] applied (0x%08X)\r\n", Buffer );
		return true;
	}
	else if(FirstVal == FLT_ASPECT_1_266 && read32(Buffer+4) == 0x44180000)
	{
		write32(Buffer, FLT_ASPECT_1_688);
		dbgprintf("PatchWidescreen:[Aspect Ratio 1.688] applied (0x%08X)\r\n", Buffer );
		return true;
	}
	else if(FirstVal == FLT_ASPECT_1_333 && (read32(Buffer+4) == 0x481c4000 || 
		read32(Buffer+4) == 0x3f800000 || read32(Buffer+4) == 0xbf800000))
	{
		write32(Buffer, FLT_ASPECT_1_777);
		dbgprintf("PatchWidescreen:[Aspect Ratio 1.777] applied (0x%08X)\r\n", Buffer );
		return true;
	}
	else if(FirstVal == FLT_ASPECT_1_357 && read32(Buffer+4) == 0x481c4000)
	{
		write32(Buffer, FLT_ASPECT_1_809);
		dbgprintf("PatchWidescreen:[Aspect Ratio 1.809] applied (0x%08X)\r\n", Buffer );
		return true;
	}
	else if(FirstVal == FLT_ASPECT_1_428 && read32(Buffer+4) == 0x3e99999a)
	{
		write32(Buffer, FLT_ASPECT_1_905);
		dbgprintf("PatchWidescreen:[Aspect Ratio 1.905] applied (0x%08X)\r\n", Buffer );
		return true;
	}
	return false;
}

extern vu32 TRIGame;
extern u32 IsN64Emu;
extern u32 DOLSize;

/**
 * Apply a static widescreen patch.
 * @param TitleID Game ID, rshifted by 8.
 * @param Region Region byte from Game ID.
 * @return True if a patch was applied; false if not.
 */
bool PatchStaticWidescreen(u32 TitleID, u32 Region)
{
	switch (TRIGame)
	{
		case TRI_GP1:
			dbgprintf("PatchWidescreen:[Mario Kart GP1] applied\r\n");
			PatchWideMulti(0x28C800, 1);
			return true;
		case TRI_GP2:
			if(read32(0x2C80D4) == 0xC03F0034)
			{
				dbgprintf("PatchWidescreen:[Mario Kart GP2 US] applied\r\n");
				PatchWideMulti(0x2C80D4, 1);
				return true;
			}
			else if(read32(0x2C86EC) == 0xC03F0034)
			{
				dbgprintf("PatchWidescreen:[Mario Kart GP2 JP] applied\r\n");
				PatchWideMulti(0x2C86EC, 1);
				return true;
			}
			return false;
		case TRI_VS4:
			if (read32(0x5E418) == 0xEC020024)
			{
				dbgprintf("PatchWidescreen:[Virtua Striker 4v06 Exp] applied\r\n");
				PatchWideMulti(0x5E418, 0); //clipping
				PatchWideMulti(0x7FA58, 0); //widescreen
				return true;
			}
			break;
		case TRI_AX:
			dbgprintf("PatchWidescreen:[F-Zero AX] applied\r\n");
			if(read32(0x445774) == FLT_ASPECT_1_333)
				write32(0x445774, read32(0x445770));
			else if(read32(0x445C34) == FLT_ASPECT_1_333)
				write32(0x445C34, read32(0x445C30));
			else if(read32(0x4461B4) == FLT_ASPECT_1_333)
				write32(0x4461B4, read32(0x4461B0));
			return true;
		default:
			break;
	}

	u32 Buffer, PatchedWide = 0;
	if(IsN64Emu)
	{
		// Special patches for N64 emulators, e.g. Ocarina of Time.
		for(Buffer = 0x90000; Buffer < 0x9F000; Buffer+=4)
		{
			if(read32(Buffer) == 0xC3A1005C && read32(Buffer+4) == 0x80030010 && read32(Buffer+8) == 0xFC80E890)
			{
				PatchWideMulti(Buffer, 29); //guPerspective
				PatchedWide++;
			}
			if(read32(Buffer) == 0xC3810014 && read32(Buffer+4) == 0x80030010 && read32(Buffer+8) == 0xFC80E090)
			{
				PatchWideMulti(Buffer, 28); //guPerspectiveF
				PatchedWide++;
			}
		}
		if(PatchedWide)
		{
			dbgprintf("PatchWidescreen:[N64 Emu] applied (%i times)\r\n", PatchedWide);
			return true;
		}
	}

	// Check that the title ID begins with 'G'.
	if (((TitleID >> 16) & 0xFF) != 'G')
	{
		// Title ID doesn't begin with 'G'.
		// No static widescreen patches available for this game.
		return false;
	}

	// Check the game ID portion of the title ID. (middle 2 characters)
	switch (TitleID & 0xFFFF)
	{
		case 0x4D34: // Mario Kart Double Dash
			dbgprintf("PatchWidescreen:[Mario Kart Double Dash] applied\r\n");
			if(Region == REGION_ID_USA || Region == REGION_ID_JAP)
			{
				PatchWideMulti(0x1D65A4, 3);
				PatchWideMulti(0x1D65FC, 2);
			}
			else if(Region == REGION_ID_EUR)
			{
				PatchWideMulti(0x1D6558, 3);
				PatchWideMulti(0x1D65B0, 2);
			}
			return true;

		case 0x4145: // Doubutsu no Mori e+
		case 0x4146: // Animal Crossing
			for(Buffer = 0x5E460; Buffer < 0x61EC0; Buffer+=4)
			{	//Every language has its own function location making 7 different locations
				if(read32(Buffer) == 0xFF801090 && read32(Buffer+4) == 0x7C9F2378)
				{
					dbgprintf("PatchWidescreen:[Animal Crossing] applied (0x%08X)\r\n", Buffer);
					PatchWideMulti(Buffer, 28);
					return true;
				}
			}
			return false;

		case 0x414C: // Super Smash Bros. Melee
			for(Buffer = 0x368500; Buffer < 0x36A500; Buffer+=4)
			{
				if(read32(Buffer) == 0x281E0000 && read32(Buffer+4) == 0xC03F0034)
				{
					dbgprintf("PatchWidescreen:[Super Smash Bros. Melee] applied (0x%08X)\r\n", Buffer+4);
					PatchWideMulti(Buffer+4, 1);
					return true;
				}
			}
			return false;

		case 0x5445: // 1080 Avalanche
			for(Buffer = 0x64000; Buffer < 0x66000; Buffer+=4)
			{
				if(read32(Buffer) == 0xEC000828 && (read32(Buffer+4) == 0xD00302A0 || read32(Buffer+4) == 0xD01C02A0))
				{
					dbgprintf("PatchWidescreen:[1080 Avalanche] applied (0x%08X)\r\n", Buffer);
					PatchWideMulti(Buffer, 0);
					PatchedWide = 1; //patching 2 areas
				}
			}
			return PatchedWide;

		case 0x5049: // Pikmin
			for(Buffer = 0x59000; Buffer < 0x5B000; Buffer+=4)
			{
				if(read32(Buffer) == 0x80BF030C)
				{
					dbgprintf("PatchWidescreen:[Pikmin] applied (0x%08X)\r\n", Buffer);
					write32(Buffer, 0x38A003AC);
					return true;
				}
			}
			return false;

		case 0x5056: // Pikmin 2
			for(Buffer = 0x424000; Buffer < 0x426000; Buffer+=4)
			{
				if(read32(Buffer) == 0xEC011824 && read32(Buffer+12) == 0xC0040000)
				{
					dbgprintf("PatchWidescreen:[Pikmin 2] applied (0x%08X)\r\n", Buffer);
					PatchWideMulti(Buffer, 0);
					return true;
				}
			}
			return false;

		case 0x4D38: // Metroid Prime
		case 0x324D: // Metroid Prime 2
			for(Buffer = 0x2B0000; Buffer < 0x3B0000; Buffer+=4)
			{
				if(read32(Buffer) == 0xFFA01090 && read32(Buffer+8) == 0xFFC01890 && read32(Buffer+12) == 0xEC250072)
				{
					PatchWideMulti(Buffer, 29); //perspective
					PatchedWide++;
				}
				if(read32(Buffer) == 0xFF401090 && read32(Buffer+4) == 0xEF210032 && read32(Buffer+12) == 0xFFC01890)
				{
					PatchWideMulti(Buffer, 26); //width near plane
					PatchWideMulti(Buffer+12, 30); //width far plane
					PatchedWide++;
				}
			}
			dbgprintf("PatchWidescreen:[Metroid Prime] applied (%i times)\r\n", PatchedWide);
			return PatchedWide;

		case 0x4832: // Need for Speed Hot Pursuit 2
			dbgprintf("PatchWidescreen:[Need for Speed Hot Pursuit 2] applied\r\n");
			if(Region == REGION_ID_USA)
			{
				write32(0x14382C, 0xC0429AE8);
				write32(0x143D58, 0xC0029AE8);
			}
			else if(Region == REGION_ID_EUR)
			{
				write32(0x1439AC, 0xC0429AE8);
				write32(0x143ED8, 0xC0029AE8);
			}
			return true;

		case 0x4C4D: // Luigi's Mansion
			dbgprintf("PatchWidescreen:[Luigis Mansion] applied\r\n");
			if(Region == REGION_ID_USA)
				PatchWideMulti(0x206A4, 0);
			else if(Region == REGION_ID_EUR)
				PatchWideMulti(0x2143C, 0);
			else if(Region == REGION_ID_JAP)
				PatchWideMulti(0x20300, 0);
			return true;

		case 0x4342: // Crash Bandicoot
			dbgprintf("PatchWidescreen:[Crash Bandicoot] clipping applied\r\n");
			if(Region == REGION_ID_USA)
				write32(0xAC768, 0xD01E0040);
			else if(Region == REGION_ID_EUR)
				write32(0xAC9A4, 0xD01E0040);
			else if(Region == REGION_ID_JAP)
				write32(0xADF1C, 0xD01E0040);
			return false; //aspect ratio gets patched later

		case 0x3258: // Sonic Gems Collection
			if(Region == REGION_ID_USA)
			{
				if(read32(0x48A2A4) == FLT_ASPECT_1_200)
				{
					dbgprintf("PatchWidescreen:[Sonic R] applied\r\n");
					//aspect ratio
					write32(0x48A2A4,FLT_ASPECT_1_600);
					//free space for new wide values
					write32(0x69124,0x7C69586E);
					write32(0x6912C,0x7E631A14);
					//general clipping left half
					write32(0x69134,0x3800FF96);
					//general clipping right half
					write32(0x69138,0x3A4002E9);
					//stage clipping multiplier
					PatchWideMulti(0x87CAC, 13);
					return true;
				}
			}
			else if(Region == REGION_ID_EUR)
			{
				if(read32(0x48A364) == FLT_ASPECT_1_200)
				{
					dbgprintf("PatchWidescreen:[Sonic R] applied\r\n");
					//aspect ratio
					write32(0x48A364,FLT_ASPECT_1_600);
					//free space for new wide values
					write32(0x69128,0x7C69586E);
					write32(0x69130,0x7E631A14);
					//general clipping left half
					write32(0x69138,0x3800FF96);
					//general clipping right half
					write32(0x6913C,0x3A4002E9);
					//stage clipping multiplier
					PatchWideMulti(0x87CB4, 13);
					return true;
				}
			}
			else if(Region == REGION_ID_JAP)
			{
				if(read32(0x48A104) == FLT_ASPECT_1_200)
				{
					dbgprintf("PatchWidescreen:[Sonic R] applied\r\n");
					//aspect ratio
					write32(0x48A104,FLT_ASPECT_1_600);
					//free space for new wide values
					write32(0x69124,0x7C69586E);
					write32(0x6912C,0x7E631A14);
					//general clipping left half
					write32(0x69134,0x3800FF96);
					//general clipping right half
					write32(0x69138,0x3A4002E9);
					//stage clipping multiplier
					PatchWideMulti(0x87BA0, 13);
					return true;
				}
			}
			return false;

		case 0x4E46: // NFL Blitz 2002
			if(Region == REGION_ID_USA)
			{
				dbgprintf("PatchWidescreen:[NFL Blitz 2002] applied\r\n");
				write32(0x199B3C,0xC0040000);
				PatchWideMulti(0x199B3C,0);
				write32(0x199B44,0xD01F0068);
				return true;
			}
			return false;

		case 0x4F33: // NFL Blitz 2003
			if(Region == REGION_ID_USA)
			{
				dbgprintf("PatchWidescreen:[NFL Blitz 2003] applied\r\n");
				write32(0x2037A8,0xC01D0000);
				PatchWideMulti(0x2037A8,0);
				write32(0x2037B0,0xD01F0068);
				return true;
			}
			return false;

		case 0x4656: // NFL Blitz Pro
			if(Region == REGION_ID_USA)
			{
				dbgprintf("PatchWidescreen:[NFL Blitz Pro] applied\r\n");
				write32(0x274114,0xC0040000);
				PatchWideMulti(0x274114,0);
				write32(0x27412C,0xD0030068);
				return true;
			}
			return false;

		case 0x4541: // Skies of Arcadia Legends
			dbgprintf("PatchWidescreen:[Skies of Arcadia Legends] applied\r\n");
			if(Region == REGION_ID_USA) {
				PatchWideMulti(0x1DCE00,2); //picture effects
				PatchWideDiv(0x299334,6); //widescreen, clipping
			} else if(Region == REGION_ID_EUR) {
				PatchWideMulti(0x1DEADC,2); //picture effects
				PatchWideDiv(0x29BC5C,6); //widescreen, clipping
			} else if(Region == REGION_ID_JAP) {
				PatchWideMulti(0x1DCCDC,1); //picture effects
				PatchWideDiv(0x298DE4,6); //widescreen, clipping
			}
			return true;

		case 0x3953: // Sonic Heroes
			// Reference: https://wiki.dolphin-emu.org/index.php?title=Sonic_Heroes#16:9_Aspect_Ratio_Fix
			if (Region == REGION_ID_USA)
			{
				if (DOLSize == 2740672-28)
				{
					// US final version.
					write32(0x42D2D4, 0x3F521DAA);
					write32(0x42DB20, 0x3F521DAA);
					write32(0x42D2E0, 0x3ED21DAA);
					return true;
				}
				else if (DOLSize == 2557472-28 && read32(0x257CF8) == 0x2E666F6E)
				{
					// Sonic Heroes 10/08/2003 prototype.
					// NOTE: The prototype uses an uncompressed font
					// file, "sega.fon", while the final version uses
					// a compressed font file, "sega.prs".
					write32(0x35F67C, 0x3F521DAA);
					write32(0x35FE68, 0x3F521DAA);
					write32(0x35F688, 0x3ED21DAA);
					return true;
				}

				// NOTE: There's at least one other known prototype,
				// dated 11/18/2003, so if the DOL size doesn't match
				// any known version, ignore it.
				return false;
			}
			else if (Region == REGION_ID_EUR)
			{
				// TODO: Not implemented.
				return false;
			}
			else if (Region == REGION_ID_JAP)
			{
				// NOTE: These addresses were found by calculating the
				// difference between .data7 in the US executable
				// compared to JP. May not be 100% accurate.
				write32(0x42D3B4, 0x3F521DAA);
				write32(0x42DC00, 0x3F521DAA);
				write32(0x42D3C0, 0x3ED21DAA);
			}

			return true;

		case 0x4B44: // Doshin the Giant
			if (Region == REGION_ID_EUR)
			{
				write32(0x17AABC, 0x3FE38E39);
				write32(0x17B0B8, 0xBF2AAAAB);
				write32(0x17B0BC, 0x3F2AAAAB);
				write32(0x17BBFC, 0xBF2AAAAB);
				write32(0x17BC00, 0x3F2AAAAB);
				return true;
			}
			return false;

		case 0x494E: // Batman Begins
		case 0x3857: // Battalion Wars
		case 0x4351: // Buffy the Vampire Slayer: Chaos Bleeds
		case 0x424F: // Burnout
		case 0x4234: // Burnout 2: Point of Impact
		case 0x5143: // Call of Duty 2: Big Red One
		case 0x4452: // Dead to Rights
		case 0x4449: // Die Hard: Vendetta
		case 0x4558: // Disney Extreme Skate Adventure
		case 0x4447: // Dragon's Lair 3D: Return to the Lair
		case 0x4439: // Drome Racers
		case 0x4544: // Eternal Darkness: Sanity's Requiem
		case 0x465A: // F-Zero GX
		case 0x4641: // FIFA Soccer 2003
		case 0x4636: // FIFA Soccer 06
		case 0x3446: // FIFA Soccer 07
		case 0x4659: // FIFA Street 2
		case 0x3646: // 2006 FIFA World Cup
		case 0x4559: // Fight Night Round 2
		case 0x4644: // Freedom Fighters
		case 0x4655: // Future Tactics: The Uprising
		case 0x4954: // Geist
		case 0x4F59: // GoldenEye: Rogue Agent
		case 0x554D: // Gun
		case 0x4E4A: // I-Ninja
		case 0x4941: // Ice Age 2: The Meltdown
		case 0x4855: // The Incredible Hulk: Ultimate Destruction
		case 0x4F37: // James Bond 007: NightFire
		case 0x4A44: // Judge Dredd: Dredd vs. Death
		case 0x4C37: // LEGO Star Wars II: The Original Trilogy
		case 0x4D44: // Madden NFL 2002
		case 0x5158: // Madden NFL 2004
		case 0x4E51: // Madden NFL 2005
		case 0x374D: // Madden NFL 07
		case 0x5138: // Madden NFL 08
		case 0x4E43: // NASCAR Thunder 2003
		case 0x4E32: // NASCAR: Dirt to Daytona
		case 0x4B33: // NBA 2K3
		case 0x4E47: // NCAA Football 2003
		case 0x4355: // NCAA Football 2005
		case 0x5735: // Need for Speed: Carbon
		case 0x5547: // Need for Speed: Underground 2
		case 0x4F57: // Need for Speed: Most Wanted
		case 0x4633: // NFL 2K3
		case 0x4E4E: // NFL Street
		case 0x3839: // Pac-Man World Rally
		case 0x574B: // Peter Jackson's King Kong: The Official Game of the Movie
		case 0x524A: // R: Racing Evolution: Life in the Fast Lane
		case 0x5248: // Rayman 3: Hoodlum Havoc
		case 0x5253: // SoulCalibur II
		case 0x5850: // Sphinx and the Cursed Mummy
		case 0x5338: // Spyro: Enter the Dragonfly
		case 0x5842: // SSX 3
		case 0x584F: // SSX On Tour
		case 0x5354: // SSX Tricky
		case 0x5341: // Star Fox Adventures
		case 0x5358: // Star Wars: The Clone Wars
		case 0x3451: // Super Mario Strikers
		case 0x4D32: // Super Monkey Ball 2
		case 0x334C: // Super Monkey Ball Adventure
		case 0x5355: // Superman: Shadow of Apokolips
		case 0x4A46: // The Adventures of Jimmy Neutron Boy Genius: Jet Fusion
		case 0x3653: // The Legend of Spyro: A New Beginning
		case 0x5051: // The Powerpuff Girls: Relish Rampage
		case 0x3346: // TimeSplitters: Future Perfect
		case 0x5434: // Tony Hawk's Pro Skater 4
		case 0x5444: // Tony Hawk's Underground
		case 0x3254: // Tony Hawk's Underground 2
		case 0x4839: // Tony Hawk's American Wasteland
		case 0x4535: // TMNT: Mutant Melee
		case 0x574C: // Wallace & Gromit in Project Zoo
			// These games all have a built-in 16:9 option.
			// Return 'true' so Nintendont doesn't attempt
			// to apply any dynamic patches.
			// Reference: https://en.wikipedia.org/wiki/List_of_GameCube_games_with_alternate_display_modes
		case 0x425A: // Resident Evil Zero
		case 0x4249: // Resident Evil Remake
		case 0x4841: // Resident Evil 2
		case 0x4C45: // Resident Evil 3: Nemesis
			// These four Resident Evil games utilize preredered backgrounds in 4:3
			// This causes the 3D objects/models to have an odd side effect
			// with the background images. 
			return true;

		default:
			// No static widescreen patches for this game.
			return false;
	}
}
