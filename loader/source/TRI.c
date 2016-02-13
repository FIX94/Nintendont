/*

Nintendont (Loader) - Playing Gamecubes in Wii mode on a Wii U

Copyright (C) 2015  FIX94

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
#include <gccore.h>
#include "exi.h"
#include "dip.h"
#include "global.h"
#include "TRI.h"

static const char *CARD_NAME_GP1 = "/saves/GP1.bin";
static const char *CARD_NAME_GP2 = "/saves/GP2.bin";
static const char *CARD_NAME_AX = "/saves/AX.bin";

static const char *SETTINGS_AX_UNK = "/saves/AX_UNKsettings.bin";
static const char *SETTINGS_AX_RVC = "/saves/AX_RVCsettings.bin";
static const char *SETTINGS_AX_RVE = "/saves/AX_RVEsettings.bin";
static const char *SETTINGS_YAKRVB = "/saves/YAKRVBsettings.bin";
static const char *SETTINGS_YAKRVC = "/saves/YAKRVCsettings.bin";
static const char *SETTINGS_VS3V02 = "/saves/VS3V02settings.bin";
static const char *SETTINGS_VS4JAP = "/saves/VS4JAPsettings.bin";
static const char *SETTINGS_VS4EXP = "/saves/VS4EXPsettings.bin";
static const char *SETTINGS_VS4V06JAP = "/saves/VS4V06JAPsettings.bin";
static const char *SETTINGS_VS4V06EXP = "/saves/VS4V06EXPsettings.bin";

u32 DOLRead32(u32 loc, u32 DOLOffset, FILE *f, u32 CurDICMD)
{
	u32 BufAtOffset = 0;
	if(f != NULL)
	{
		fseek(f, DOLOffset+loc, SEEK_SET);
		fread(&BufAtOffset, 1, 4, f);
	}
	else if(CurDICMD)
	{
		ReadRealDisc((u8*)&BufAtOffset, DOLOffset+loc, 4, CurDICMD);
	}
	return BufAtOffset;
}

u32 TRISetupGames(char *Path, u32 CurDICMD, u32 ISOShift)
{
	u32 res = 0;
	FILE *f = NULL;
	u32 DOLOffset = 0;
	if(CurDICMD)
	{
		ReadRealDisc((u8*)&DOLOffset, 0x420+ISOShift, 4, CurDICMD);
		DOLOffset+=ISOShift;
	}
	else
	{
		char FullPath[300];
		snprintf(FullPath, sizeof(FullPath), "%s:%s", GetRootDevice(), Path);
		f = fopen(FullPath, "rb");
		if(f != NULL)
		{
			fseek(f, 0x420+ISOShift, SEEK_SET);
			fread(&DOLOffset, 1, 4, f);
			DOLOffset+=ISOShift;
		}
		else
		{
			char FSTPath[300];
			snprintf(FSTPath, sizeof(FSTPath), "%ssys/main.dol", FullPath);
			f = fopen(FSTPath, "rb");
		}
		if(f == NULL)
			return 0;
	}

	char SaveFile[128];
	if(DOLRead32(0x210320, DOLOffset, f, CurDICMD) == 0x386000A8)
	{
		res = 1;
		gprintf("TRI:Mario Kart GP1\r\n");
		snprintf(SaveFile, sizeof(SaveFile), "%s:%s", GetRootDevice(), CARD_NAME_GP1);
		CreateNewFile(SaveFile, 0x45);
	}
	else if(DOLRead32(0x25C0AC, DOLOffset, f, CurDICMD) == 0x386000A8)
	{
		res = 1;
		gprintf("TRI:Mario Kart GP2\r\n");
		snprintf(SaveFile, sizeof(SaveFile), "%s:%s", GetRootDevice(), CARD_NAME_GP2);
		CreateNewFile(SaveFile, 0x45);
	}
	else if(DOLRead32(0x1821C4, DOLOffset, f, CurDICMD) == 0x386000A8)
	{
		res = 1;
		gprintf("TRI:F-Zero AX (Unk)\r\n");
		snprintf(SaveFile, sizeof(SaveFile), "%s:%s", GetRootDevice(), CARD_NAME_AX);
		CreateNewFile(SaveFile, 0xCF);
		snprintf(SaveFile, sizeof(SaveFile), "%s:%s", GetRootDevice(), SETTINGS_AX_UNK);
		CreateNewFile(SaveFile, 0x2A);
	}
	else if(DOLRead32(0x181E60, DOLOffset, f, CurDICMD) == 0x386000A8)
	{
		res = 1;
		gprintf("TRI:F-Zero AX (Rev C)\r\n");
		snprintf(SaveFile, sizeof(SaveFile), "%s:%s", GetRootDevice(), CARD_NAME_AX);
		CreateNewFile(SaveFile, 0xCF);
		snprintf(SaveFile, sizeof(SaveFile), "%s:%s", GetRootDevice(), SETTINGS_AX_RVC);
		CreateNewFile(SaveFile, 0x2A);
	}
	else if(DOLRead32(0x18275C, DOLOffset, f, CurDICMD) == 0x386000A8)
	{
		res = 1;
		gprintf("TRI:F-Zero AX (Rev E)\r\n");
		snprintf(SaveFile, sizeof(SaveFile), "%s:%s", GetRootDevice(), CARD_NAME_AX);
		CreateNewFile(SaveFile, 0xCF);
		snprintf(SaveFile, sizeof(SaveFile), "%s:%s", GetRootDevice(), SETTINGS_AX_RVE);
		CreateNewFile(SaveFile, 0x2A);
	}
	else if(DOLRead32(0x01C2DF4, DOLOffset, f, CurDICMD) == 0x386000A8)
	{
		res = 1;
		gprintf("TRI:Virtua Striker 3 Ver 2002\r\n");
		snprintf(SaveFile, sizeof(SaveFile), "%s:%s", GetRootDevice(), SETTINGS_VS3V02);
		CreateNewFile(SaveFile, 0x12);
	}
	else if(DOLRead32(0x01CF1C4, DOLOffset, f, CurDICMD) == 0x386000A8)
	{
		res = 1;
		gprintf("TRI:Virtua Striker 4 (Japan)\r\n");
		snprintf(SaveFile, sizeof(SaveFile), "%s:%s", GetRootDevice(), SETTINGS_VS4JAP);
		CreateNewFile(SaveFile, 0x2B);
	}
	else if(DOLRead32(0x1C5514, DOLOffset, f, CurDICMD) == 0x386000A8)
	{
		res = 1;
		gprintf("TRI:Virtua Striker 4 (Export)\r\n");
		snprintf(SaveFile, sizeof(SaveFile), "%s:%s", GetRootDevice(), SETTINGS_VS4EXP);
		CreateNewFile(SaveFile, 0x2B);
	}
	else if(DOLRead32(0x24B248, DOLOffset, f, CurDICMD) == 0x386000A8)
	{
		res = 1;
		gprintf("TRI:Virtua Striker 4 Ver 2006 (Japan)\r\n");
		snprintf(SaveFile, sizeof(SaveFile), "%s:%s", GetRootDevice(), SETTINGS_VS4V06JAP);
		CreateNewFile(SaveFile, 0x2E);
	}
	else if(DOLRead32(0x20D7E8, DOLOffset, f, CurDICMD) == 0x386000A8)
	{
		res = 1;
		gprintf("TRI:Virtua Striker 4 Ver 2006 (Export)\r\n");
		snprintf(SaveFile, sizeof(SaveFile), "%s:%s", GetRootDevice(), SETTINGS_VS4V06EXP);
		CreateNewFile(SaveFile, 0x2B);
	}
	else if(DOLRead32(0x26B3F4, DOLOffset, f, CurDICMD) == 0x386000A8)
	{
		res = 1;
		gprintf("TRI:Gekitou Pro Yakyuu (Rev B)\r\n");
		snprintf(SaveFile, sizeof(SaveFile), "%s:%s", GetRootDevice(), SETTINGS_YAKRVB);
		CreateNewFile(SaveFile, 0xF5);
	}
	else if(DOLRead32(0x26D9B4, DOLOffset, f, CurDICMD) == 0x386000A8)
	{
		res = 1;
		gprintf("TRI:Gekitou Pro Yakyuu (Rev C)\r\n");
		snprintf(SaveFile, sizeof(SaveFile), "%s:%s", GetRootDevice(), SETTINGS_YAKRVC);
		CreateNewFile(SaveFile, 0x100);
	}

	if(f != NULL)
		fclose(f);
	return res;
}
