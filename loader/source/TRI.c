/*

Nintendont (Loader) - Playing Gamecubes in Wii mode on a Wii U

Copyright (C) 2015-2016  FIX94

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
#include "ff_utf8.h"
#include "wdvd.h"

static const char CARD_NAME_GP1[] = "/saves/GP1.bin";
static const char CARD_NAME_GP2[] = "/saves/GP2.bin";
static const char CARD_NAME_GP2J[] = "/saves/GP2J.bin";
static const char CARD_NAME_AX[] = "/saves/AX.bin";

static const char SETTINGS_AX_RVC[] = "/saves/AX_RVCsettings.bin";
static const char SETTINGS_AX_RVD[] = "/saves/AX_RVDsettings.bin";
static const char SETTINGS_AX_RVE[] = "/saves/AX_RVEsettings.bin";
static const char SETTINGS_YAKRVB[] = "/saves/YAKRVBsettings.bin";
static const char SETTINGS_YAKRVC[] = "/saves/YAKRVCsettings.bin";
static const char SETTINGS_VS3V02[] = "/saves/VS3V02settings.bin";
static const char SETTINGS_VS4JAP[] = "/saves/VS4JAPsettings.bin";
static const char SETTINGS_VS4EXP[] = "/saves/VS4EXPsettings.bin";
static const char SETTINGS_VS4V06JAP[] = "/saves/VS4V06JAPsettings.bin";
static const char SETTINGS_VS4V06EXP[] = "/saves/VS4V06EXPsettings.bin";

extern bool wiiVCInternal;

static u32 DOLRead32(u32 loc, u32 DOLOffset, FIL *f, unsigned int CurDICMD)
{
	u32 BufAtOffset = 0;
	if(wiiVCInternal)
	{
		WDVD_FST_LSeek(DOLOffset+loc);
		WDVD_FST_Read(wdvdTmpBuf, 4);
		memcpy(&BufAtOffset, wdvdTmpBuf, 4);
	}
	else if(f != NULL)
	{
		UINT read;
		f_lseek(f, DOLOffset+loc);
		f_read(f, &BufAtOffset, 4, &read);
	}
	else if(CurDICMD)
		ReadRealDisc((u8*)&BufAtOffset, DOLOffset+loc, 4, CurDICMD);
	return BufAtOffset;
}

u32 TRISetupGames(char *Path, unsigned int CurDICMD, u32 ISOShift)
{
	u32 res = 0;
	u32 DOLOffset = 0;
	FIL f;
	FIL *fp = NULL;
	UINT read;
	FRESULT fres = FR_DISK_ERR;

	if(CurDICMD)
	{
		ReadRealDisc((u8*)&DOLOffset, 0x420+ISOShift, 4, CurDICMD);
		DOLOffset+=ISOShift;
	}
	else if(wiiVCInternal)
	{
		WDVD_FST_OpenDisc(0);
		WDVD_FST_LSeek(0x420+ISOShift);
		WDVD_FST_Read(wdvdTmpBuf, 4);
		memcpy(&DOLOffset, wdvdTmpBuf, 4);
		DOLOffset+=ISOShift;
	}
	else
	{
		char FullPath[260];
		snprintf(FullPath, sizeof(FullPath), "%s:%s", GetRootDevice(), Path);
		fres = f_open_char(&f, FullPath, FA_READ|FA_OPEN_EXISTING);
		if (fres == FR_OK)
		{
			f_lseek(&f, 0x420+ISOShift);
			f_read(&f, &DOLOffset, 4, &read);
			DOLOffset+=ISOShift;
		}
		else
		{
			char FSTPath[260];
			snprintf(FSTPath, sizeof(FSTPath), "%ssys/main.dol", FullPath);
			fres = f_open_char(&f, FSTPath, FA_READ|FA_OPEN_EXISTING);
		}

		if (fres != FR_OK)
			return 0;
		fp = &f;
	}

	// Create the save file if it doesn't already exist.
	char SaveFile[128];
	if(DOLRead32(0x210320, DOLOffset, fp, CurDICMD) == 0x386000A8)
	{
		res = 1;
		gprintf("TRI:Mario Kart Arcade GP (ENG Feb 14 2006 13:09:48)\r\n");
		snprintf(SaveFile, sizeof(SaveFile), "%s:%s", GetRootDevice(), CARD_NAME_GP1);
		CreateNewFile(SaveFile, 0x45);
	}
	else if(DOLRead32(0x25C0AC, DOLOffset, fp, CurDICMD) == 0x386000A8)
	{
		res = 1;
		gprintf("TRI:Mario Kart Arcade GP 2 (ENG Feb 7 2007 02:47:24)\r\n");
		snprintf(SaveFile, sizeof(SaveFile), "%s:%s", GetRootDevice(), CARD_NAME_GP2);
		CreateNewFile(SaveFile, 0x45);
	}
	else if(DOLRead32(0x25C664, DOLOffset, fp, CurDICMD) == 0x386000A8)
	{
		res = 1;
		gprintf("TRI:Mario Kart Arcade GP 2 (JPN Feb 6 2007 20:29:25)\r\n");
		snprintf(SaveFile, sizeof(SaveFile), "%s:%s", GetRootDevice(), CARD_NAME_GP2J);
		CreateNewFile(SaveFile, 0x45);
	}
	else if(DOLRead32(0x181E60, DOLOffset, fp, CurDICMD) == 0x386000A8)
	{
		res = 1;
		gprintf("TRI:F-Zero AX (Rev C)\r\n");
		snprintf(SaveFile, sizeof(SaveFile), "%s:%s", GetRootDevice(), CARD_NAME_AX);
		CreateNewFile(SaveFile, 0xCF);
		snprintf(SaveFile, sizeof(SaveFile), "%s:%s", GetRootDevice(), SETTINGS_AX_RVC);
		CreateNewFile(SaveFile, 0x2A);
	}
	else if(DOLRead32(0x1821C4, DOLOffset, fp, CurDICMD) == 0x386000A8)
	{
		res = 1;
		gprintf("TRI:F-Zero AX (Rev D)\r\n");
		snprintf(SaveFile, sizeof(SaveFile), "%s:%s", GetRootDevice(), CARD_NAME_AX);
		CreateNewFile(SaveFile, 0xCF);
		snprintf(SaveFile, sizeof(SaveFile), "%s:%s", GetRootDevice(), SETTINGS_AX_RVD);
		CreateNewFile(SaveFile, 0x2A);
	}
	else if(DOLRead32(0x18275C, DOLOffset, fp, CurDICMD) == 0x386000A8)
	{
		res = 1;
		gprintf("TRI:F-Zero AX (Rev E)\r\n");
		snprintf(SaveFile, sizeof(SaveFile), "%s:%s", GetRootDevice(), CARD_NAME_AX);
		CreateNewFile(SaveFile, 0xCF);
		snprintf(SaveFile, sizeof(SaveFile), "%s:%s", GetRootDevice(), SETTINGS_AX_RVE);
		CreateNewFile(SaveFile, 0x2A);
	}
	else if(DOLRead32(0x01C2DF4, DOLOffset, fp, CurDICMD) == 0x386000A8)
	{
		res = 1;
		gprintf("TRI:Virtua Striker 3 Ver 2002\r\n");
		snprintf(SaveFile, sizeof(SaveFile), "%s:%s", GetRootDevice(), SETTINGS_VS3V02);
		CreateNewFile(SaveFile, 0x12);
	}
	else if(DOLRead32(0x01CF1C4, DOLOffset, fp, CurDICMD) == 0x386000A8)
	{
		res = 1;
		gprintf("TRI:Virtua Striker 4 (Japan)\r\n");
		snprintf(SaveFile, sizeof(SaveFile), "%s:%s", GetRootDevice(), SETTINGS_VS4JAP);
		CreateNewFile(SaveFile, 0x2B);
	}
	else if(DOLRead32(0x1C51E4, DOLOffset, fp, CurDICMD) == 0x386000A8)
	{
		res = 1;
		gprintf("TRI:Virtua Striker 4 (Export) (GDT-0014)\r\n");
		snprintf(SaveFile, sizeof(SaveFile), "%s:%s", GetRootDevice(), SETTINGS_VS4EXP);
		CreateNewFile(SaveFile, 0x2B);
	}
	else if(DOLRead32(0x1C5514, DOLOffset, fp, CurDICMD) == 0x386000A8)
	{
		res = 1;
		gprintf("TRI:Virtua Striker 4 (Export) (GDT-0015)\r\n");
		snprintf(SaveFile, sizeof(SaveFile), "%s:%s", GetRootDevice(), SETTINGS_VS4EXP);
		CreateNewFile(SaveFile, 0x2B);
	}
	else if(DOLRead32(0x24A4C8, DOLOffset, fp, CurDICMD) == 0x386000A8)
	{
		res = 1;
		gprintf("TRI:Virtua Striker 4 Ver 2006 (Japan) (Rev B)\r\n");
		snprintf(SaveFile, sizeof(SaveFile), "%s:%s", GetRootDevice(), SETTINGS_VS4V06JAP);
		CreateNewFile(SaveFile, 0x2E);
	}
	else if(DOLRead32(0x24B248, DOLOffset, fp, CurDICMD) == 0x386000A8)
	{
		res = 1;
		gprintf("TRI:Virtua Striker 4 Ver 2006 (Japan) (Rev D)\r\n");
		snprintf(SaveFile, sizeof(SaveFile), "%s:%s", GetRootDevice(), SETTINGS_VS4V06JAP);
		CreateNewFile(SaveFile, 0x2E);
	}
	else if(DOLRead32(0x20D7E8, DOLOffset, fp, CurDICMD) == 0x386000A8)
	{
		res = 1;
		gprintf("TRI:Virtua Striker 4 Ver 2006 (Export)\r\n");
		snprintf(SaveFile, sizeof(SaveFile), "%s:%s", GetRootDevice(), SETTINGS_VS4V06EXP);
		CreateNewFile(SaveFile, 0x2B);
	}
	else if(DOLRead32(0x26B3F4, DOLOffset, fp, CurDICMD) == 0x386000A8)
	{
		res = 1;
		gprintf("TRI:Gekitou Pro Yakyuu (Rev B)\r\n");
		snprintf(SaveFile, sizeof(SaveFile), "%s:%s", GetRootDevice(), SETTINGS_YAKRVB);
		CreateNewFile(SaveFile, 0xF5);
	}
	else if(DOLRead32(0x26D9B4, DOLOffset, fp, CurDICMD) == 0x386000A8)
	{
		res = 1;
		gprintf("TRI:Gekitou Pro Yakyuu (Rev C)\r\n");
		snprintf(SaveFile, sizeof(SaveFile), "%s:%s", GetRootDevice(), SETTINGS_YAKRVC);
		CreateNewFile(SaveFile, 0x100);
	}

	if(wiiVCInternal)
		WDVD_FST_Close();
	else if (fres == FR_OK)
		f_close(&f);
	return res;
}
