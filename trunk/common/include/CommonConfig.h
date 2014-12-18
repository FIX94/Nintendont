
#ifndef __COMMON_CONFIG_H__
#define __COMMON_CONFIG_H__

#include "NintendontVersion.h"
#include "Metadata.h"

#define NIN_CFG_VERSION		0x00000003

#define NIN_CFG_MAXPAD 4

typedef struct NIN_CFG
{
	unsigned int		Magicbytes;		// 0x01070CF6
	unsigned int		Version;		// 0x00000001
	unsigned int		Config;
	unsigned int		VideoMode;
	unsigned int		Language;
	char	GamePath[255];
	char	CheatPath[255];
	unsigned int		MaxPads;
	unsigned int		GameID;
	unsigned int		MemCardBlocks;
} NIN_CFG;

enum ninconfigbitpos
{
	NIN_CFG_BIT_CHEATS		= (0),
	NIN_CFG_BIT_DEBUGGER	= (1),	// Only for Wii Version
	NIN_CFG_BIT_DEBUGWAIT	= (2),	// Only for Wii Version
	NIN_CFG_BIT_MEMCARDEMU	= (3),
	NIN_CFG_BIT_CHEAT_PATH	= (4),
	NIN_CFG_BIT_FORCE_WIDE	= (5),
	NIN_CFG_BIT_FORCE_PROG	= (6),
	NIN_CFG_BIT_AUTO_BOOT	= (7),
	NIN_CFG_BIT_HID			= (8),
	NIN_CFG_BIT_OSREPORT	= (9),
	NIN_CFG_BIT_USB			= (10),
	NIN_CFG_BIT_LED			= (11),
	NIN_CFG_BIT_LOG			= (12),
	NIN_CFG_BIT_LAST		= (13),

	NIN_CFG_BIT_MC_MULTI	= (13),
	NIN_CFG_BIT_NATIVE_SI	= (14),
	NIN_CFG_BIT_WIIU_WIDE	= (15),
};

enum ninconfig
{
	NIN_CFG_CHEATS		= (1<<NIN_CFG_BIT_CHEATS),
	NIN_CFG_DEBUGGER	= (1<<NIN_CFG_BIT_DEBUGGER),	// Only for Wii Version
	NIN_CFG_DEBUGWAIT	= (1<<NIN_CFG_BIT_DEBUGWAIT),	// Only for Wii Version
	NIN_CFG_MEMCARDEMU	= (1<<NIN_CFG_BIT_MEMCARDEMU),
	NIN_CFG_CHEAT_PATH	= (1<<NIN_CFG_BIT_CHEAT_PATH),
	NIN_CFG_FORCE_WIDE	= (1<<NIN_CFG_BIT_FORCE_WIDE),
	NIN_CFG_FORCE_PROG	= (1<<NIN_CFG_BIT_FORCE_PROG),
	NIN_CFG_AUTO_BOOT	= (1<<NIN_CFG_BIT_AUTO_BOOT),
	NIN_CFG_HID			= (1<<NIN_CFG_BIT_HID),
	NIN_CFG_OSREPORT	= (1<<NIN_CFG_BIT_OSREPORT),
	NIN_CFG_USB			= (1<<NIN_CFG_BIT_USB),
	NIN_CFG_LED			= (1<<NIN_CFG_BIT_LED),
	NIN_CFG_LOG			= (1<<NIN_CFG_BIT_LOG),
	NIN_CFG_MC_MULTI	= (1<<NIN_CFG_BIT_MC_MULTI),
	NIN_CFG_NATIVE_SI	= (1<<NIN_CFG_BIT_NATIVE_SI),
	NIN_CFG_WIIU_WIDE	= (1<<NIN_CFG_BIT_WIIU_WIDE),
};

enum ninextrasettings
{
	NIN_SETTINGS_MAX_PADS	= NIN_CFG_BIT_LAST,
	NIN_SETTINGS_LANGUAGE,
	NIN_SETTINGS_VIDEO,
	NIN_SETTINGS_VIDEOMODE,
	NIN_SETTINGS_MEMCARDBLOCKS,
	NIN_SETTINGS_MEMCARDMULTI,
	NIN_SETTINGS_NATIVE_SI,
	NIN_SETTINGS_LAST,
};

enum ninvideomodeindex
{
	NIN_VID_INDEX_AUTO		= (0),
	NIN_VID_INDEX_FORCE		= (1),
	NIN_VID_INDEX_NONE		= (2),
	NIN_VID_INDEX_FORCE_DF	= (4),
	NIN_VID_INDEX_FORCE_PAL50	= (0),
	NIN_VID_INDEX_FORCE_PAL60	= (1),
	NIN_VID_INDEX_FORCE_NTSC	= (2),
	NIN_VID_INDEX_FORCE_MPAL	= (3),
};

enum ninvideomode
{
	NIN_VID_AUTO		= (NIN_VID_INDEX_AUTO    <<16),
	NIN_VID_FORCE		= (NIN_VID_INDEX_FORCE   <<16),
	NIN_VID_NONE		= (NIN_VID_INDEX_NONE    <<16),
	NIN_VID_FORCE_DF	= (NIN_VID_INDEX_FORCE_DF<<16),

	NIN_VID_MASK		= NIN_VID_AUTO|NIN_VID_FORCE|NIN_VID_NONE|NIN_VID_FORCE_DF,

	NIN_VID_FORCE_PAL50	= (1<<NIN_VID_INDEX_FORCE_PAL50),
	NIN_VID_FORCE_PAL60	= (1<<NIN_VID_INDEX_FORCE_PAL60),
	NIN_VID_FORCE_NTSC	= (1<<NIN_VID_INDEX_FORCE_NTSC),
	NIN_VID_FORCE_MPAL	= (1<<NIN_VID_INDEX_FORCE_MPAL),

	NIN_VID_FORCE_MASK	= NIN_VID_FORCE_PAL50|NIN_VID_FORCE_PAL60|NIN_VID_FORCE_NTSC|NIN_VID_FORCE_MPAL,

	NIN_VID_PROG		= (1<<4),	//important to prevent blackscreens
};

enum ninlanguage
{
	NIN_LAN_ENGLISH		= 0,
	NIN_LAN_GERMAN		= 1,
	NIN_LAN_FRENCH		= 2,
	NIN_LAN_SPANISH		= 3,
	NIN_LAN_ITALIAN		= 4,
	NIN_LAN_DUTCH		= 5,

	NIN_LAN_FIRST		= 0,
	NIN_LAN_LAST		= 6, 
/* Auto will use English for E/P region codes and 
   only other languages when these region codes are used: D/F/S/I/J  */

	NIN_LAN_AUTO		= -1, 
};

enum VideoModes
{
	GCVideoModeNone		= 0,
	GCVideoModePAL60	= 1,
	GCVideoModeNTSC		= 2,
	GCVideoModePROG		= 3,
};


//Mem0059 = 0, 0x04, 0x0080000
//Mem0123 = 1, 0x08, 0x0100000
//Mem0251 = 2, 0x10, 0x0200000
//Mem0507 = 3, 0x20, 0x0400000
//Mem1019 = 4, 0x40, 0x0800000
//Mem2043 = 5, 0x80, 0x1000000
#define MEM_CARD_MAX (5)
#define MEM_CARD_CODE(x) (1<<(x+2))
#define MEM_CARD_SIZE(x) (1<<(x+19))
#define MEM_CARD_BLOCKS(x) ((1<<(x+6))-5)

#endif
