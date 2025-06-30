#ifndef __COMMON_CONFIG_H__
#define __COMMON_CONFIG_H__

#include "NintendontVersion.h"
#include "Metadata.h"

#define NIN_CFG_VERSION         0x0000000A

#define NIN_CFG_MAXPAD 4

typedef struct NIN_CFG
{
        unsigned int            Magicbytes;             // 0x01070CF6
        unsigned int            Version;                // 0x00000001
        unsigned int            Config;
        unsigned int            VideoMode;
        unsigned int            Language;
        char    GamePath[255];
        char    CheatPath[255];
        unsigned int            MaxPads;
        unsigned int            GameID;
        unsigned char           MemCardBlocks;
        signed char                     VideoScale;
        signed char                     VideoOffset;
        unsigned char           NetworkProfile;
        unsigned int            WiiUGamepadSlot;
} NIN_CFG;

enum ninconfigbitpos
{
        NIN_CFG_BIT_CHEATS      = (0),
        NIN_CFG_BIT_DEBUGGER    = (1),  // Only for Wii Version
        NIN_CFG_BIT_DEBUGWAIT   = (2),  // Only for Wii Version
        NIN_CFG_BIT_MEMCARDEMU  = (3),
        NIN_CFG_BIT_CHEAT_PATH  = (4),
        NIN_CFG_BIT_FORCE_WIDE  = (5),
        NIN_CFG_BIT_FORCE_PROG  = (6),
        NIN_CFG_BIT_AUTO_BOOT   = (7),
        NIN_CFG_BIT_HID         = (8),  // Old Versions
        NIN_CFG_BIT_REMLIMIT    = (8),  // New Versions
        NIN_CFG_BIT_OSREPORT    = (9),
        NIN_CFG_BIT_USB         = (10),
        NIN_CFG_BIT_LED         = (11),
        NIN_CFG_BIT_LOG         = (12),
        NIN_CFG_BIT_LAST        = (13),

        NIN_CFG_BIT_MC_MULTI    = (13),
        NIN_CFG_BIT_NATIVE_SI   = (14),
        NIN_CFG_BIT_WIIU_WIDE   = (15),
        NIN_CFG_BIT_ARCADE_MODE = (16),
        NIN_CFG_BIT_CC_RUMBLE   = (17),
        NIN_CFG_BIT_SKIP_IPL    = (18),
        NIN_CFG_BIT_BBA_EMU             = (19),

        // Internal kernel settings.
        NIN_CFG_BIT_MC_SLOTB    = (31), // Slot B image is loaded
};

enum ninconfig
{
        NIN_CFG_CHEATS          = (1<<NIN_CFG_BIT_CHEATS),
        NIN_CFG_DEBUGGER        = (1<<NIN_CFG_BIT_DEBUGGER),    // Only for Wii Version
        NIN_CFG_DEBUGWAIT       = (1<<NIN_CFG_BIT_DEBUGWAIT),   // Only for Wii Version
        NIN_CFG_MEMCARDEMU      = (1<<NIN_CFG_BIT_MEMCARDEMU),
        NIN_CFG_CHEAT_PATH      = (1<<NIN_CFG_BIT_CHEAT_PATH),
        NIN_CFG_FORCE_WIDE      = (1<<NIN_CFG_BIT_FORCE_WIDE),
        NIN_CFG_FORCE_PROG      = (1<<NIN_CFG_BIT_FORCE_PROG),
        NIN_CFG_AUTO_BOOT       = (1<<NIN_CFG_BIT_AUTO_BOOT),
        NIN_CFG_HID             = (1<<NIN_CFG_BIT_HID),
        NIN_CFG_REMLIMIT        = (1<<NIN_CFG_BIT_REMLIMIT),
        NIN_CFG_OSREPORT        = (1<<NIN_CFG_BIT_OSREPORT),
        NIN_CFG_USB             = (1<<NIN_CFG_BIT_USB),
        NIN_CFG_LED             = (1<<NIN_CFG_BIT_LED),         // Only for Wii Version
        NIN_CFG_LOG             = (1<<NIN_CFG_BIT_LOG),

        NIN_CFG_MC_MULTI        = (1<<NIN_CFG_BIT_MC_MULTI),
        NIN_CFG_NATIVE_SI       = (1<<NIN_CFG_BIT_NATIVE_SI),   // Only for Wii Version
        NIN_CFG_WIIU_WIDE       = (1<<NIN_CFG_BIT_WIIU_WIDE),   // Only for Wii U Version
        NIN_CFG_ARCADE_MODE     = (1<<NIN_CFG_BIT_ARCADE_MODE),
        NIN_CFG_CC_RUMBLE       = (1<<NIN_CFG_BIT_CC_RUMBLE),
        NIN_CFG_SKIP_IPL        = (1<<NIN_CFG_BIT_SKIP_IPL),
        NIN_CFG_BBA_EMU         = (1<<NIN_CFG_BIT_BBA_EMU),

        NIN_CFG_MC_SLOTB        = (1<<NIN_CFG_BIT_MC_SLOTB),
};

enum ninextrasettings
{
        NIN_SETTINGS_MAX_PADS   = NIN_CFG_BIT_LAST,
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
//high bits
        NIN_VID_INDEX_AUTO                      = (0),
        NIN_VID_INDEX_FORCE                     = (1),
        NIN_VID_INDEX_NONE                      = (2),
        NIN_VID_INDEX_FORCE_DF          = (4),
//low bits
        NIN_VID_INDEX_FORCE_PAL50       = (0),
        NIN_VID_INDEX_FORCE_PAL60       = (1),
        NIN_VID_INDEX_FORCE_NTSC        = (2),
        NIN_VID_INDEX_FORCE_MPAL        = (3),

        NIN_VID_INDEX_PROG                      = (4),
        NIN_VID_INDEX_PATCH_PAL50       = (5),
        // Indices for 240p/288p modes
        NIN_VID_INDEX_FORCE_NTSC_240P    = (6),
        // NIN_VID_INDEX_FORCE_PAL_288P     = (7), // Removed
        NIN_VID_INDEX_FORCE_MPAL_240P    = (8), // Adjusted index
        NIN_VID_INDEX_FORCE_EURGB60_240P = (9), // Adjusted index
        // Index for 576p mode
        // NIN_VID_INDEX_FORCE_PAL_576P     = (10), // Removed
};

enum ninvideomode
{
        NIN_VID_AUTO            = (NIN_VID_INDEX_AUTO           <<16),
        NIN_VID_FORCE           = (NIN_VID_INDEX_FORCE          <<16), // This is the general "force" flag for VideoMode field
        NIN_VID_NONE            = (NIN_VID_INDEX_NONE           <<16),
        NIN_VID_FORCE_DF        = (NIN_VID_INDEX_FORCE_DF       <<16),

        NIN_VID_MASK            = NIN_VID_AUTO|NIN_VID_FORCE|NIN_VID_NONE|NIN_VID_FORCE_DF, // Mask for the high bits

        // Specific video mode flags (low bits)
        NIN_VID_FORCE_PAL50     = (1<<NIN_VID_INDEX_FORCE_PAL50),       // Bit 0 (0x01)
        NIN_VID_FORCE_PAL60     = (1<<NIN_VID_INDEX_FORCE_PAL60),       // Bit 1 (0x02)
        NIN_VID_FORCE_NTSC      = (1<<NIN_VID_INDEX_FORCE_NTSC),        // Bit 2 (0x04)
        NIN_VID_FORCE_MPAL      = (1<<NIN_VID_INDEX_FORCE_MPAL),        // Bit 3 (0x08)

        NIN_VID_PROG            = (1<<NIN_VID_INDEX_PROG),              // Bit 4 (0x10) important to prevent blackscreens
        NIN_VID_PATCH_PAL50     = (1<<NIN_VID_INDEX_PATCH_PAL50),       // Bit 5 (0x20) different force behaviour

        // 240p/288p modes
        NIN_VID_FORCE_NTSC_240P    = (1<<NIN_VID_INDEX_FORCE_NTSC_240P),    // Bit 6 (0x40)
        // NIN_VID_FORCE_PAL_288P     = (1<<NIN_VID_INDEX_FORCE_PAL_288P),     // Removed
        NIN_VID_FORCE_MPAL_240P    = (1<<NIN_VID_INDEX_FORCE_MPAL_240P),    // Bit 8 (0x100) - Note: Bit position remains if others are removed
        NIN_VID_FORCE_EURGB60_240P = (1<<NIN_VID_INDEX_FORCE_EURGB60_240P), // Bit 9 (0x200) - Note: Bit position remains

        // 576p mode
        // NIN_VID_FORCE_PAL_576P     = (1<<NIN_VID_INDEX_FORCE_PAL_576P),     // Removed


        // NIN_VID_FORCE_MASK defines which of the low bits are considered for forced modes.
        // It's used in main.c: specificForceFlags = (ncfg->VideoMode & NIN_VID_FORCE_MASK);
        // This needs to include all individual force flags.
        NIN_VID_FORCE_MASK      = NIN_VID_FORCE_PAL50 | NIN_VID_FORCE_PAL60 | NIN_VID_FORCE_NTSC | NIN_VID_FORCE_MPAL |
                                  NIN_VID_FORCE_NTSC_240P | NIN_VID_FORCE_MPAL_240P | NIN_VID_FORCE_EURGB60_240P,
                                  // Mask now covers bits 0,1,2,3, 6,8,9
};

enum ninlanguage
{
        NIN_LAN_ENGLISH         = 0,
        NIN_LAN_GERMAN          = 1,
        NIN_LAN_FRENCH          = 2,
        NIN_LAN_SPANISH         = 3,
        NIN_LAN_ITALIAN         = 4,
        NIN_LAN_DUTCH           = 5,

        NIN_LAN_FIRST           = 0,
        NIN_LAN_LAST            = 6,
/* Auto will use English for E/P region codes and
   only other languages when these region codes are used: D/F/S/I/J  */

        NIN_LAN_AUTO            = -1,
};

enum VideoModes
{
        GCVideoModeNone         = 0,
        GCVideoModePAL60        = 1,
        GCVideoModeNTSC         = 2,
        GCVideoModePROG         = 3,
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

// bi2.bin region codes. (0x458)
enum BI2region_codes
{
        BI2_REGION_JAPAN        = 0,
        BI2_REGION_USA          = 1,
        BI2_REGION_PAL          = 2,
        BI2_REGION_SOUTH_KOREA  = 4,
};

#endif
