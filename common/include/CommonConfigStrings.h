#ifndef __COMMON_CONFIG_STRINGS_H__
#define __COMMON_CONFIG_STRINGS_H__

//Strings must match order in CommonConfig.h
const char* OptionStrings[] =
{
        "Cheats",
        "Debugger",
        "Debugger Wait",
        "Memcard Emulation",
        "Cheat Path",
        "Force Widescreen",
        "Force Progressive",
        "Auto Boot",
        "Unlock Read Speed",
        "OSReport",
        "WiiU Widescreen", //Replaces USB
        "Drive Access LED",
        "Log",

        "MaxPads",
        "Language",
        "Video",
        "Videomode", // This is the "Force Video Mode" setting
        "Memcard Blocks",
        "Memcard Multi",
        "Native Control",
};

const char* LanguageStrings[] =
{
        "Eng",
        "Ger",
        "Fre",
        "Spa",
        "Ita",
        "Dut",

        "Auto",
};

const char* VideoStrings[] =
{
        "Auto",
        "Force",
        "None",
        "Invalid",
        "Force (Deflicker)",
};

// Represents the options for "Force Video Mode" (NIN_SETTINGS_VIDEOMODE)
// Order must match videoModeFlagMap in menu.c
const char* VideoModeStrings[] =
{
        "PAL50",    // Corresponds to NIN_VID_FORCE_PAL50 (bit 0)
        "PAL60",    // Corresponds to NIN_VID_FORCE_PAL60 (bit 1)
        "NTSC",     // Corresponds to NIN_VID_FORCE_NTSC  (bit 2)
        "MPAL",     // Corresponds to NIN_VID_FORCE_MPAL  (bit 3)
        "NTSC 240p",// Corresponds to NIN_VID_FORCE_NTSC_240P (bit 6)
        "PAL 288p", // Corresponds to NIN_VID_FORCE_PAL_288P  (bit 7)
        "MPAL 240p",// Corresponds to NIN_VID_FORCE_MPAL_240P (bit 8)
        "PAL60 240p",// Corresponds to NIN_VID_FORCE_EURGB60_240P (bit 9)
        "PAL 576p"  // Corresponds to NIN_VID_FORCE_PAL_576P (bit 10)
};
#define NUM_VIDEOMODE_STRINGS (sizeof(VideoModeStrings) / sizeof(char*))

#endif
