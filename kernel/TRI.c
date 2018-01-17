/*

Nintendont (Kernel) - Playing Gamecubes in Wii mode on a Wii U

Copyright (C) 2013  crediar
Copyright (C) 2014-2016  FIX94

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
#include "Patch.h"
#include "string.h"
#include "TRI.h"
#include "GCAM.h"
#include "DI.h"
#include "EXI.h"
#include "ff_utf8.h"
#include "Config.h"
#include "asm/PADReadGP.h"
#include "asm/PADReadF.h"
#include "asm/PADReadVS3.h"
#include "asm/PADReadVS.h"
#include "asm/CheckTestMenu.h"
#include "asm/CheckTestMenuGP.h"
#include "asm/CheckTestMenuVS.h"
#include "asm/CheckTestMenuYakyuu.h"
#include "asm/RestoreSettingsAX_RVC.h"
#include "asm/RestoreSettingsAX_RVD.h"
#include "asm/RestoreSettingsAX_RVE.h"
#include "asm/RestoreSettingsYAKRVB.h"
#include "asm/RestoreSettingsYAKRVC.h"
#include "asm/RestoreSettingsVS3V02.h"
#include "asm/RestoreSettingsVS4JAP.h"
#include "asm/RestoreSettingsVS4EXP.h"
#include "asm/RestoreSettingsVS4V06JAP.h"
#include "asm/RestoreSettingsVS4V06EXP.h"
#include "asm/PADControlMotorGP.h"

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

static const char *TRISettingsName = (char*)0;
static u32 TRISettingsLoc = 0, TRISettingsSize = 0;

static void *OUR_SETTINGS_LOC = (void*)0x13003500;

#ifndef DEBUG_PATCH
#define dbgprintf(...)
#else
extern int dbgprintf( const char *fmt, ...);
#endif

extern vu32 useipltri, TRI_BackupAvailable;
extern u32 SystemRegion;

extern u32 GAME_ID;
extern u16 GAME_ID6;

vu32 TRIGame = TRI_NONE;
extern vu32 AXTimerOffset;

u32 arcadeMode = 0;
void TRIInit()
{
	GCAMInit();
	arcadeMode = ConfigGetConfig(NIN_CFG_ARCADE_MODE);
}

void TRIReset()
{
	//Reset GCAM status
	GCAMInit();
	//F-Zero AX uses Clean CARD after 150 uses
	if(TRIGame == TRI_AX && TRI_BackupAvailable == 1)
	{
		//if we dont set it to 150 it'll beep a lot
		sync_before_read(OUR_SETTINGS_LOC, 0x20);
		W16((u32)OUR_SETTINGS_LOC+0x16,150);
		sync_after_write(OUR_SETTINGS_LOC, 0x20);
	}
}

void TRIBackupSettings()
{
	if(TRISettingsName == (char*)0 || TRISettingsLoc == 0 || TRISettingsSize == 0)
		return;
	void *GameSettingsLoc = (TRIGame == TRI_AX || TRIGame == TRI_YAK) ? (void*)TRISettingsLoc : (void*)P2C(read32(TRISettingsLoc));
	sync_before_read_align32(GameSettingsLoc, TRISettingsSize);
	sync_before_read_align32(OUR_SETTINGS_LOC, TRISettingsSize);
	if((memcmp(OUR_SETTINGS_LOC, GameSettingsLoc, TRISettingsSize) != 0) && (memcmp(GameSettingsLoc, "SB", 2) == 0))
	{
		dbgprintf("TRI:Writing Settings\r\n");
		memcpy(OUR_SETTINGS_LOC, GameSettingsLoc, TRISettingsSize);
		FIL backup;
		if(f_open_char(&backup, TRISettingsName, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK)
		{
			u32 wrote;
			f_write(&backup, OUR_SETTINGS_LOC, TRISettingsSize, &wrote);
			f_close(&backup);
		}
		sync_after_write_align32(OUR_SETTINGS_LOC, TRISettingsSize);
		TRI_BackupAvailable = 1;
	}
}

void TRIReadSettings()
{
	if(TRISettingsName == (char*)0 || TRISettingsSize == 0)
		return;
	FIL backup;
	if (f_open_char(&backup, TRISettingsName, FA_OPEN_EXISTING | FA_READ) == FR_OK)
	{
		if(backup.obj.objsize == TRISettingsSize)
		{
			u32 read;
			u8 sbuf[TRISettingsSize];
			f_read(&backup, sbuf, TRISettingsSize, &read);
			if(memcmp(sbuf, "SB", 2) == 0)
			{
				dbgprintf("TRI:Reading Settings\r\n");
				memcpy(OUR_SETTINGS_LOC, sbuf, TRISettingsSize);
				sync_after_write_align32(OUR_SETTINGS_LOC, TRISettingsSize);
				TRI_BackupAvailable = 1;
			}
		}
		f_close(&backup);
	}
}

void TRISetupGames()
{
	if( read32(0x023D240) == 0x386000A8 )
	{
		dbgprintf("TRI:Mario Kart Arcade GP (ENG Feb 14 2006 13:09:48)\r\n");
		TRIGame = TRI_GP1;
		SystemRegion = REGION_EXPORT;

		//Unlimited CARD uses
		write32( 0x01F5C44, 0x60000000 );

		//Allow test menu if requested
		PatchBL( PatchCopy(CheckTestMenuGP, CheckTestMenuGP_size), 0x31BE8 );

		//dont wait for other cabinets to link up
		write32( 0x00328F4, 0x38600000 );

		//Disable wheel/handle
		//write32( 0x007909C, 0x98650022 );

		//Disable cam
		write32( 0x00790A0, 0x98650025 );

		//VS wait
		write32( 0x00BE10C, 0x4800002C );

		if(!arcadeMode)
		{
			//Remove some menu timers
			write32( 0x0019BFF8, 0x60000000 ); //card check
			write32( 0x001BCAA8, 0x60000000 ); //want to make a card
			write32( 0x00195748, 0x60000000 ); //card view
			write32( 0x000CFABC, 0x60000000 ); //select game mode
			write32( 0x000D9F14, 0x60000000 ); //select character
			write32( 0x001A8CF8, 0x60000000 ); //select cup
			write32( 0x001A89B8, 0x60000000 ); //select round
			write32( 0x001A36CC, 0x60000000 ); //select item pack (card)
			write32( 0x001A2A10, 0x60000000 ); //select item (card)
			write32( 0x001B9724, 0x60000000 ); //continue
			write32( 0x001E61B4, 0x60000000 ); //rewrite rank
			write32( 0x001A822C, 0x60000000 ); //select course p1 (time attack)
			write32( 0x001A7F0C, 0x60000000 ); //select course p2 (time attack)
			write32( 0x000D6234, 0x60000000 ); //enter name (time attack, card)
			write32( 0x001BF1DC, 0x60000000 ); //save record p1 (time attack on card)
			write32( 0x001BF1DC, 0x60000000 ); //save record p2 (time attack on card)
			write32( 0x000E01B4, 0x60000000 ); //select record place (time attack on card)

			//Make some menu timers invisible
			PatchB( 0x000B12EC, 0x000B12E0 );

			//Make coin count (layer 7) invisible
			write32( 0x0008650C, 0x4E800020 );
		}

		//Modify to regular GX pattern to patch later
		write32( 0x363660, 0x00 ); //NTSC Interlaced

		//Test Menu Interlaced
		//memcpy( (void*)0x036369C, (void*)0x040EB88, 0x3C );

		//PAD Hook for control updates
		PatchBL( PatchCopy(PADReadGP, PADReadGP_size), 0x3C2A0 );

		// Game specific rumble hook
		PatchBL( PatchCopy(PADControlMotorGP, PADControlMotorGP_size), 0x3AA40 );

		//some report check skip
		//write32( 0x00307CC, 0x60000000 );

		//memcpy( (void*)0x00330BC, OSReportDM, sizeof(OSReportDM) );
		//memcpy( (void*)0x0030710, OSReportDM, sizeof(OSReportDM) );
		//memcpy( (void*)0x0030760, OSReportDM, sizeof(OSReportDM) );
		//memcpy( (void*)0x020CBCC, OSReportDM, sizeof(OSReportDM) );	// UNkReport
		//memcpy( (void*)0x02281B8, OSReportDM, sizeof(OSReportDM) );	// UNkReport4
	}
	else if( read32(0x02856EC) == 0x386000A8 )
	{
		dbgprintf("TRI:Mario Kart Arcade GP 2 (ENG Feb 7 2007 02:47:24)\r\n");
		TRIGame = TRI_GP2;
		SystemRegion = REGION_EXPORT;

		//Unlimited CARD uses
		write32( 0x00A02F8, 0x60000000 );

		//Allow test menu if requested
		PatchBL( PatchCopy(CheckTestMenuGP, CheckTestMenuGP_size), 0x2E338 );

		//dont wait for other cabinets to link up
		write32( 0x002F054, 0x38600000 );

		//Disable wheel
		//write32( 0x0073BD4, 0x98650022 );

		//Disable cam
		write32( 0x0073BD8, 0x98650025 );

		//Disable red item button
		write32( 0x0073C98, 0x98BF0045 );

		//VS wait patch 
		write32( 0x0084FC4, 0x4800000C );
		write32( 0x0085000, 0x60000000 );

		if(!arcadeMode)
		{
			//Remove some menu timers (thanks conanac)
			write32( 0x001C1074, 0x60000000 ); //card check
			write32( 0x001C0174, 0x60000000 ); //want to make a card
			write32( 0x00232818, 0x60000000 ); //card view
			write32( 0x001C1A08, 0x60000000 ); //select game mode
			write32( 0x001C3380, 0x60000000 ); //select character
			write32( 0x001D7594, 0x60000000 ); //select kart
			write32( 0x001C7A7C, 0x60000000 ); //select cup
			write32( 0x001C9ED8, 0x60000000 ); //select round
			write32( 0x00237B3C, 0x60000000 ); //select item (card)
			write32( 0x0024D35C, 0x60000000 ); //continue
			write32( 0x0015F2F4, 0x60000000 ); //rewrite rank
			write32( 0x001CF5DC, 0x60000000 ); //select course (time attack)
			write32( 0x001BE248, 0x60000000 ); //enter name (time attack, card)
			write32( 0x0021DFF0, 0x60000000 ); //save record (time attack on card)

			//Make some menu timers invisible
			write32( 0x001B7D2C, 0x60000000 );
			write32( 0x00231CA0, 0x60000000 );

			//Make coin count (layer 7) invisible
			write32( 0x001B87A8, 0x60000000 );
			write32( 0x00247A68, 0x60000000 );
		}

		//Modify to regular GX pattern to patch later
		write32( 0x3F1FD0, 0x00 ); //NTSC Interlaced

		//PAD Hook for control updates
		PatchBL( PatchCopy(PADReadGP, PADReadGP_size), 0x38A34 );

		// Game specific rumble hook
		PatchBL( PatchCopy(PADControlMotorGP, PADControlMotorGP_size), 0x3717C );

		//memcpy( (void*)0x002CE3C, OSReportDM, sizeof(OSReportDM) );
		//memcpy( (void*)0x002CE8C, OSReportDM, sizeof(OSReportDM) );
		//write32( 0x002CEF8, 0x60000000 );
	}
	else if( read32(0x0285D04) == 0x386000A8 )
	{
		dbgprintf("TRI:Mario Kart Arcade GP 2 (JPN Feb 6 2007 20:29:25)\r\n");
		TRIGame = TRI_GP2;
		SystemRegion = REGION_JAPAN;

		//Unlimited CARD uses
		write32( 0x00A0910, 0x60000000 );

		//Allow test menu if requested
		PatchBL( PatchCopy(CheckTestMenuGP, CheckTestMenuGP_size), 0x2E398 );

		//dont wait for other cabinets to link up
		write32( 0x002F0B4, 0x38600000 );

		//Disable cam
		write32( 0x00741F0, 0x98C50025 );

		//Disable red item button
		write32( 0x00742B0, 0x98BF0045 );

		//VS wait patch 
		write32( 0x00855DC, 0x4800000C );
		write32( 0x0085618, 0x60000000 );

		if(!arcadeMode)
		{
			//Remove some menu timers (thanks conanac)
			write32( 0x001C1664, 0x60000000 ); //card check
			write32( 0x001C0764, 0x60000000 ); //want to make a card
			write32( 0x00232E20, 0x60000000 ); //card view
			write32( 0x001C1FF8, 0x60000000 ); //select game mode
			write32( 0x001C3970, 0x60000000 ); //select character
			write32( 0x001D7B84, 0x60000000 ); //select kart
			write32( 0x001C806C, 0x60000000 ); //select cup
			write32( 0x001CA4C8, 0x60000000 ); //select round
			write32( 0x00238144, 0x60000000 ); //select item (card)
			write32( 0x0024D964, 0x60000000 ); //continue
			write32( 0x0015F90C, 0x60000000 ); //rewrite rank
			write32( 0x001CFBCC, 0x60000000 ); //select course (time attack)
			write32( 0x001BE838, 0x60000000 ); //enter name (time attack, card)
			write32( 0x0021E5F8, 0x60000000 ); //save record (time attack on card)

			//Make some menu timers invisible
			write32( 0x001B831C, 0x60000000 );
			write32( 0x002322A8, 0x60000000 );

			//Make coin count (layer 7) invisible
			write32( 0x001B8D98, 0x60000000 );
			write32( 0x00248070, 0x60000000 );
		}

		//Modify to regular GX pattern to patch later
		write32( 0x3F25F0, 0x00 ); //NTSC Interlaced

		//PAD Hook for control updates
		PatchBL( PatchCopy(PADReadGP, PADReadGP_size), 0x3904C );

		// Game specific rumble hook
		PatchBL( PatchCopy(PADControlMotorGP, PADControlMotorGP_size), 0x37794 );
	}
	else if( read32(0x0184E60) == 0x386000A8 )
	{
		dbgprintf("TRI:F-Zero AX (Rev C)\r\n");
		TRIGame = TRI_AX;
		SystemRegion = REGION_JAPAN;
		AXTimerOffset = 0x003CD1C0;
		TRISettingsName = SETTINGS_AX_RVC;
		TRISettingsLoc = 0x3CF6F0;
		TRISettingsSize = 0x2A;
		DISetDIMMVersion(0x12301777);

		//Reset loop
		write32( 0x01B50AC, 0x60000000 );

		//DBGRead fix
		write32( 0x01BEAAC, 0x60000000 );

		//Motor init patch
		write32( 0x01753BC, 0x60000000 );
		write32( 0x01753C0, 0x60000000 );
		write32( 0x0175358, 0x60000000 );

		//patching system waiting stuff
		write32( 0x0180A54, 0x48000054 );
		write32( 0x0180AB8, 0x48000054 );

		//Network waiting
		write32( 0x0180C74, 0x4800004C );

		//Goto Test menu
		write32( 0x00DF168, 0x60000000 );
		//Allow test menu if requested
		PatchBL( PatchCopy(CheckTestMenu, CheckTestMenu_size), 0xDF16C );

		//Remove Overscan on first VIConfigure
		write32( 0x001FB998, 0x38C00000 );

		//English
		write32( 0x000DF430, 0x38000000 );

		if(!arcadeMode)
		{
			//Remove some menu timers (thanks dj_skual)
			// (menu gets constantly removed in JVSIO.c)
			write32( 0x0015AE40, 0x60000000 ); //after race

			//Make some menu timers invisible (thanks dj_skual)
			write32( 0x002370DC, 0x40200000 ); //menu inner
			write32( 0x00237114, 0x40200000 ); //menu outer
			write32( 0x00236E74, 0x00000000 ); //after race inner
			write32( 0x00236EAC, 0x00000000 ); //after race outer
		}

		//Check for already existing settings
		if(TRI_BackupAvailable == 0)
			TRIReadSettings();
		//Custom backup handler
		if(TRI_BackupAvailable == 1)
			PatchB(PatchCopy(RestoreSettingsAX_RVC, RestoreSettingsAX_RVC_size), 0x17B12C);

		//Replace Motor Init with Controller Setup
		write32( 0x0024DB74, 0x801809F8 );
		write32( 0x0024DB78, 0x8018085C );

		//Call Important Init Function
		PatchB( 0x000DA46C, 0x00180A38 );

		//Modify to regular GX pattern to patch later
		u32 NTSC480ProgTri = 0x21CF2C;
		write32(NTSC480ProgTri, 0x00); //NTSC Interlaced
		write32(NTSC480ProgTri + 0x14, 0x01); //Mode DF

		NTSC480ProgTri = 0x302600;
		write32(NTSC480ProgTri, 0x00); //NTSC Interlaced
		write32(NTSC480ProgTri + 0x14, 0x01); //Mode DF

		//Make sure GX modes get set on force PAL60 as well
		PatchB(0x8FFF0, 0x90110);
		PatchB(0x1FB92C, 0x1FB96C);

		//PAD Hook for control updates
		PatchBL( PatchCopy(PADReadF, PADReadF_size), 0x1B4004 );
	}
	else if( read32(0x01851C4) == 0x386000A8 )
	{
		dbgprintf("TRI:F-Zero AX (Rev D)\r\n");
		TRIGame = TRI_AX;
		SystemRegion = REGION_JAPAN;
		AXTimerOffset = 0x003CD6A0;
		TRISettingsName = SETTINGS_AX_RVD;
		TRISettingsLoc = 0x3CFBD0;
		TRISettingsSize = 0x2A;
		DISetDIMMVersion(0x12301777);

		//Reset loop
		write32( 0x01B5410, 0x60000000 );

		//DBGRead fix
		write32( 0x01BEF38, 0x60000000 );

		//Disable CARD
		//write32( 0x017B2BC, 0x38C00000 );

		//Motor init patch
		write32( 0x0175710, 0x60000000 );
		write32( 0x0175714, 0x60000000 );
		write32( 0x01756AC, 0x60000000 );

		//patching system waiting stuff
		write32( 0x0180DB8, 0x48000054 );
		write32( 0x0180E1C, 0x48000054 );

		//Network waiting
		write32( 0x0180FD8, 0x4800004C );

		//Goto Test menu
		write32( 0x00DF3D0, 0x60000000 );
		//Allow test menu if requested
		PatchBL( PatchCopy(CheckTestMenu, CheckTestMenu_size), 0xDF3D4 );

		//Replace Motor Init with Controller Setup
		write32( 0x0024E034, 0x80180D5C );
		write32( 0x0024E038, 0x80180BC0 );

		//Call Important Init Function
		PatchB( 0x000DA6D4, 0x00180D9C );

		//Remove Overscan on first VIConfigure
		write32( 0x001FBE54, 0x38C00000 );

		//English
		write32( 0x000DF698, 0x38000000 );

		if(!arcadeMode)
		{
			//Remove some menu timers (thanks dj_skual)
			// (menu gets constantly removed in JVSIO.c)
			write32( 0x0015B148, 0x60000000 ); //after race

			//Make some menu timers invisible (thanks dj_skual)
			write32( 0x0023759C, 0x40200000 ); //menu inner
			write32( 0x002375D4, 0x40200000 ); //menu outer
			write32( 0x00237334, 0x00000000 ); //after race inner
			write32( 0x0023736C, 0x00000000 ); //after race outer
		}

		//Check for already existing settings
		if(TRI_BackupAvailable == 0)
			TRIReadSettings();
		//Custom backup handler
		if(TRI_BackupAvailable == 1)
			PatchB(PatchCopy(RestoreSettingsAX_RVD, RestoreSettingsAX_RVD_size), 0x17B490);

		//Modify to regular GX pattern to patch later
		u32 NTSC480ProgTri = 0x21D3EC;
		write32(NTSC480ProgTri, 0x00); //NTSC Interlaced
		write32(NTSC480ProgTri + 0x14, 0x01); //Mode DF

		NTSC480ProgTri = 0x302AC0;
		write32(NTSC480ProgTri, 0x00); //NTSC Interlaced
		write32(NTSC480ProgTri + 0x14, 0x01); //Mode DF

		//Make sure GX modes get set on force PAL60 as well
		PatchB(0x900F8, 0x90218);
		PatchB(0x1FBDE8, 0x1FBE28);

		//PAD Hook for control updates
		PatchBL( PatchCopy(PADReadF, PADReadF_size), 0x1B4368 );

		//memcpy( (void*)0x01CAACC, patch_fwrite_GC, sizeof(patch_fwrite_GC) );
		//memcpy( (void*)0x01882C0, OSReportDM, sizeof(OSReportDM) );

		//Unkreport
		//PatchB( 0x01882C0, 0x0191B54 );
		//PatchB( 0x01882C0, 0x01C53CC );
		//PatchB( 0x01882C0, 0x01CC684 );
	}
	else if( read32(0x018575C) == 0x386000A8 )
	{
		dbgprintf("TRI:F-Zero AX (Rev E)\r\n");
		TRIGame = TRI_AX;
		SystemRegion = REGION_JAPAN;
		AXTimerOffset = 0x003CDC20;
		TRISettingsName = SETTINGS_AX_RVE;
		TRISettingsLoc = 0x3D0150;
		TRISettingsSize = 0x2A;
		DISetDIMMVersion(0x12301777);

		//Reset loop
		write32( 0x01B59A8, 0x60000000 );

		//DBGRead fix
		write32( 0x01BF3A8, 0x60000000 );

		//Motor init patch
		write32( 0x0175C00, 0x60000000 );
		write32( 0x0175C04, 0x60000000 );
		write32( 0x0175B9C, 0x60000000 );

		//patching system waiting stuff
		write32( 0x0181344, 0x48000054 );
		write32( 0x01813A8, 0x48000054 );

		//Network waiting
		write32( 0x0181564, 0x4800004C );

		//Goto Test menu
		write32( 0x00DF550, 0x60000000 );
		//Allow test menu if requested
		PatchBL( PatchCopy(CheckTestMenu, CheckTestMenu_size), 0xDF554 );

		//Replace Motor Init with Controller Setup
		write32( 0x0024E4B4, 0x801812E8 );
		write32( 0x0024E4B8, 0x8018114C );

		//Call Important Init Function
		PatchB( 0x000DA81C, 0x00181328 );

		//Remove Overscan on first VIConfigure
		write32( 0x001FC2C4, 0x38C00000 );

		//English
		write32( 0x000DF818, 0x38000000 );

		if(!arcadeMode)
		{
			//Remove some menu timers (thanks dj_skual)
			// (menu gets constantly removed in JVSIO.c)
			write32( 0x0015B638, 0x60000000 ); //after race

			//Make some menu timers invisible (thanks dj_skual)
			write32( 0x00237A1C, 0x40200000 ); //menu inner
			write32( 0x00237A54, 0x40200000 ); //menu outer
			write32( 0x002377B4, 0x00000000 ); //after race inner
			write32( 0x002377EC, 0x00000000 ); //after race outer
		}

		//Check for already existing settings
		if(TRI_BackupAvailable == 0)
			TRIReadSettings();
		//Custom backup handler
		if(TRI_BackupAvailable == 1)
			PatchB(PatchCopy(RestoreSettingsAX_RVE, RestoreSettingsAX_RVE_size), 0x17B980);

		//Modify to regular GX pattern to patch later
		u32 NTSC480ProgTri = 0x21D86C;
		write32(NTSC480ProgTri, 0x00); //NTSC Interlaced
		write32(NTSC480ProgTri + 0x14, 0x01); //Mode DF

		NTSC480ProgTri = 0x303040;
		write32(NTSC480ProgTri, 0x00); //NTSC Interlaced
		write32(NTSC480ProgTri + 0x14, 0x01); //Mode DF

		//Make sure GX modes get set on force PAL60 as well
		PatchB(0x90140, 0x90260);
		PatchB(0x1FC258, 0x1FC298);

		//PAD Hook for control updates
		PatchBL( PatchCopy(PADReadF, PADReadF_size), 0x1B4900 );
	}
	else if( read32( 0x01C6EF4 ) == 0x386000A8 )
	{
		dbgprintf("TRI:Virtua Striker 3 Ver 2002\r\n");
		TRIGame = TRI_VS3;

		if(GAME_ID == 0x47565333 && GAME_ID6 == 0x324A)
		{	//Japanese Language
			SystemRegion = REGION_JAPAN;
		}
		else
		{	//English Language
			SystemRegion = REGION_EXPORT;
		}
		TRISettingsName = SETTINGS_VS3V02;
		TRISettingsLoc = 0x05D25C0-0x1408;
		TRISettingsSize = 0x12;

		//Allow test menu if requested
		PatchBL( PatchCopy(CheckTestMenuVS, CheckTestMenuVS_size), 0x541E4 );

		//Check for already existing settings
		if(TRI_BackupAvailable == 0)
			TRIReadSettings();
		//Custom backup handler
		if(TRI_BackupAvailable == 1)
			PatchB(PatchCopy(RestoreSettingsVS3V02, RestoreSettingsVS3V02_size), 0x1C60FC);

		//make one instruction free for out custom call to store settings
		write32( 0x1C5E38, read32(0x1C5E40) ); //lwz r4, -0x1408(r13)
		//dcbst r7 (0 from memset), r4 (0x05D25C0-0x1408 from write32 above)
		write32( 0x1C5E40, 0x7C07206C );

		/* all of the 6 patches below are needed to make the
		media board i/o conform to our emulation in DI.c */

		//do write to media board first
		PatchB(0x2356D8,0x23563C);
		//do read from media board next
		PatchB(0x23564C,0x235778);
		//make sure to read into the correct area
		write32(0x23564C,0x3BDEFFE0);
		//also make sure to read back the correct media board offset
		write32(0x235660,0x8366000C);
		//done reading from media board, jump to successful end
		PatchB(0x2357B8,0x2356C0);
		//dont start media board threads which would only mess up
		write32(0x23790C,0x60000000);

		//Modify to regular GX pattern to patch later
		write32( 0x26E7E8, 0x00 ); //NTSC Interlaced

		//PAD Hook for control updates
		PatchBL(PatchCopy(PADReadVS3, PADReadVS3_size), 0x5472C );
	}
	else if( read32( 0x01D2564 ) == 0x386000A8 )
	{
		dbgprintf("TRI:Virtua Striker 4 (Japan)\r\n");
		TRIGame = TRI_VS4;
		SystemRegion = REGION_JAPAN;
		TRISettingsName = SETTINGS_VS4JAP;
		TRISettingsLoc = 0x0602520-0x14C4;
		TRISettingsSize = 0x2B;

		if(!arcadeMode)
		{
			//Set menu timer to about 51 days
			write32( 0x00C1D0C, 0x3C800FFF );
		}

		//Allow test menu if requested
		PatchBL( PatchCopy(CheckTestMenuVS, CheckTestMenuVS_size), 0x3B444 );

		//Check for already existing settings
		if(TRI_BackupAvailable == 0)
			TRIReadSettings();
		//Custom backup handler
		if(TRI_BackupAvailable == 1)
			PatchB(PatchCopy(RestoreSettingsVS4JAP, RestoreSettingsVS4JAP_size), 0x12698);

		//Modify to regular GX pattern to patch later
		write32( 0x231310, 0x00 ); //NTSC Interlaced

		//PAD Hook for control updates
		PatchBL(PatchCopy(PADReadVS, PADReadVS_size), 0x3C230 );
	}
	else if( read32( 0x01C8584 ) == 0x386000A8 )
	{
		dbgprintf("TRI:Virtua Striker 4 (Export GDT-0014)\r\n");
		TRIGame = TRI_VS4;
		SystemRegion = REGION_USA;
		TRISettingsName = SETTINGS_VS4EXP;
		TRISettingsLoc = 0x05C5CA0-0x2CA8; //NOTE:logic turned around!
		TRISettingsSize = 0x2B;

		if(!arcadeMode)
		{
			//Set menu timer to about 51 days
			write32( 0x00C14E0, 0x3C800FFF );
		}

		//Allow test menu if requested
		PatchBL( PatchCopy(CheckTestMenuVS, CheckTestMenuVS_size), 0x3B3E8 );

		//Check for already existing settings
		if(TRI_BackupAvailable == 0)
			TRIReadSettings();
		//Custom backup handler
		if(TRI_BackupAvailable == 1)
			PatchB(PatchCopy(RestoreSettingsVS4EXP, RestoreSettingsVS4EXP_size), 0x12454);

		//Modify to regular GX pattern to patch later
		write32( 0x21D468, 0x00 ); //NTSC Interlaced

		//PAD Hook for control updates
		PatchBL(PatchCopy(PADReadVS, PADReadVS_size), 0x3C174 );
	}
	else if( read32( 0x01C88B4 ) == 0x386000A8 )
	{
		dbgprintf("TRI:Virtua Striker 4 (Export GDT-0015)\r\n");
		TRIGame = TRI_VS4;
		SystemRegion = REGION_USA;
		TRISettingsName = SETTINGS_VS4EXP;
		TRISettingsLoc = 0x05C5FE0-0x2CA8; //NOTE:logic turned around!
		TRISettingsSize = 0x2B;

		if(!arcadeMode)
		{
			//Set menu timer to about 51 days
			write32( 0x00C1564, 0x3C800FFF );
		}

		//Allow test menu if requested
		PatchBL( PatchCopy(CheckTestMenuVS, CheckTestMenuVS_size), 0x3B3E8 );

		//Check for already existing settings
		if(TRI_BackupAvailable == 0)
			TRIReadSettings();
		//Custom backup handler
		if(TRI_BackupAvailable == 1)
			PatchB(PatchCopy(RestoreSettingsVS4EXP, RestoreSettingsVS4EXP_size), 0x12454);

		//Modify to regular GX pattern to patch later
		write32( 0x21D7A8, 0x00 ); //NTSC Interlaced

		//PAD Hook for control updates
		PatchBL(PatchCopy(PADReadVS, PADReadVS_size), 0x3C174 );
	}
	else if( read32( 0x024DB08 ) == 0x386000A8 )
	{
		dbgprintf("TRI:Virtua Striker 4 Ver 2006 (Japan) (Rev B)\r\n");
		TRIGame = TRI_VS4;
		SystemRegion = REGION_JAPAN;
		TRISettingsName = SETTINGS_VS4V06JAP;
		TRISettingsLoc = 0x06D4C80-0xC38;
		TRISettingsSize = 0x2E;
		DISetDIMMVersion(0xA3A479);

		//DIMM memory format skip (saves 2 minutes bootup time)
		write32( 0x0013950, 0x60000000 );

		//dont wait for other cabinets to link up
		write32( 0x0056114, 0x38600001 );

		if(!arcadeMode)
		{
			//Set menu timer to about 51 days
			write32( 0x00D6BC8, 0x3C800FFF );
		}

		//Allow test menu if requested
		PatchBL( PatchCopy(CheckTestMenuVS, CheckTestMenuVS_size), 0x3ECD8 );

		//Check for already existing settings
		if(TRI_BackupAvailable == 0)
			TRIReadSettings();
		//Custom backup handler
		if(TRI_BackupAvailable == 1)
			PatchB(PatchCopy(RestoreSettingsVS4V06JAP, RestoreSettingsVS4V06JAP_size), 0x13C10);

		//Modify to regular GX pattern to patch later
		write32( 0x2BC7D8, 0x00 ); //NTSC Interlaced

		//PAD Hook for control updates
		PatchBL(PatchCopy(PADReadVS, PADReadVS_size), 0x3F9C8 );
	}
	else if( read32( 0x024E888 ) == 0x386000A8 )
	{
		dbgprintf("TRI:Virtua Striker 4 Ver 2006 (Japan) (Rev D)\r\n");
		TRIGame = TRI_VS4;
		SystemRegion = REGION_JAPAN;
		TRISettingsName = SETTINGS_VS4V06JAP;
		TRISettingsLoc = 0x06D5A40-0xCDC;
		TRISettingsSize = 0x2E;
		DISetDIMMVersion(0xA3A479);

		//DIMM memory format skip (saves 2 minutes bootup time)
		write32( 0x0013950, 0x60000000 );

		//dont wait for other cabinets to link up
		write32( 0x0056114, 0x38600001 );

		if(!arcadeMode)
		{
			//Set menu timer to about 51 days
			write32( 0x00D7420, 0x3C800FFF );
		}

		//Allow test menu if requested
		PatchBL( PatchCopy(CheckTestMenuVS, CheckTestMenuVS_size), 0x3ECD8 );

		//Check for already existing settings
		if(TRI_BackupAvailable == 0)
			TRIReadSettings();
		//Custom backup handler
		if(TRI_BackupAvailable == 1)
			PatchB(PatchCopy(RestoreSettingsVS4V06JAP, RestoreSettingsVS4V06JAP_size), 0x13C10);

		//Modify to regular GX pattern to patch later
		write32( 0x2BD598, 0x00 ); //NTSC Interlaced

		//PAD Hook for control updates
		PatchBL(PatchCopy(PADReadVS, PADReadVS_size), 0x3F9C8 );
	}
	else if( read32( 0x0210C08 ) == 0x386000A8 )
	{
		dbgprintf("TRI:Virtua Striker 4 Ver 2006 (Export)\r\n");
		TRIGame = TRI_VS4;
		SystemRegion = REGION_USA;
		TRISettingsName = SETTINGS_VS4V06EXP;
		TRISettingsLoc = 0x0684B40-0xEE4;
		TRISettingsSize = 0x2B;
		DISetDIMMVersion(0xA3A479);

		if(!arcadeMode)
		{
			//Set menu timer to about 51 days
			write32( 0x00CBB7C, 0x3C800FFF );

			//Hide timer updater (red font)
			PatchBL( 0x011BDEC, 0x001738E0 );
			PatchBL( 0x011BDEC, 0x00173950 );

			//Hide timer updater (blue font)
			PatchBL( 0x011BDEC, 0x001739D4 );
			PatchBL( 0x011BDEC, 0x00173A44 );
		}

		//Allow test menu if requested
		PatchBL( PatchCopy(CheckTestMenuVS, CheckTestMenuVS_size), 0x3B804 );

		//Check for already existing settings
		if(TRI_BackupAvailable == 0)
			TRIReadSettings();
		//Custom backup handler
		if(TRI_BackupAvailable == 1)
			PatchB(PatchCopy(RestoreSettingsVS4V06EXP, RestoreSettingsVS4V06EXP_size), 0x12BD4);

		//Modify to regular GX pattern to patch later
		write32( 0x26DD38, 0x00 ); //NTSC Interlaced

		//PAD Hook for control updates
		PatchBL(PatchCopy(PADReadVS, PADReadVS_size), 0x3C504 );

		//memcpy( (void*)0x001C2B80, OSReportDM, sizeof(OSReportDM) );
	}
	else if(read32(0x2995F4) == 0x386000A8)
	{
		dbgprintf("TRI:Gekitou Pro Yakyuu (Rev B)\r\n");
		TRIGame = TRI_YAK;
		SystemRegion = REGION_JAPAN;
		TRISettingsName = SETTINGS_YAKRVB;
		TRISettingsLoc = 0x03D3D0C;
		TRISettingsSize = 0xF5;

		//Allow test menu if requested
		PatchBL( PatchCopy(CheckTestMenuYakyuu, CheckTestMenuYakyuu_size), 0x1D1EF8 );

		//Check for already existing settings
		if(TRI_BackupAvailable == 0)
			TRIReadSettings();
		//Custom backup handler
		if(TRI_BackupAvailable == 1)
			PatchBL(PatchCopy(RestoreSettingsYAKRVB, RestoreSettingsYAKRVB_size), 0x1D21F8);

		//PAD Hook for control updates
		PatchBL(PatchCopy(PADReadGP, PADReadGP_size), 0x1C380C );
	}
	else if(read32(0x29BC94) == 0x386000A8)
	{
		dbgprintf("TRI:Gekitou Pro Yakyuu (Rev C)\r\n");
		TRIGame = TRI_YAK;
		SystemRegion = REGION_JAPAN;
		TRISettingsName = SETTINGS_YAKRVC;
		TRISettingsLoc = 0x03D67AC;
		TRISettingsSize = 0x100;

		//Allow test menu if requested
		PatchBL( PatchCopy(CheckTestMenuYakyuu, CheckTestMenuYakyuu_size), 0x1D455C );

		//Check for already existing settings
		if(TRI_BackupAvailable == 0)
			TRIReadSettings();
		//Custom backup handler
		if(TRI_BackupAvailable == 1)
			PatchBL(PatchCopy(RestoreSettingsYAKRVC, RestoreSettingsYAKRVC_size), 0x1D486C);

		//PAD Hook for control updates
		PatchBL(PatchCopy(PADReadGP, PADReadGP_size), 0x1C575C );
	}
	else if(useipltri)
	{
		dbgprintf("TRI:SegaBoot\r\n");
		TRIGame = TRI_SB;
	}
	//actual game is booting up, prepare
	if(TRIGame != TRI_NONE && TRIGame != TRI_SB)
		EXIPrepareTRIGameStart();
}
