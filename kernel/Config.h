
#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "global.h"
#include "string.h"
#include "common.h"
#include "alloc.h"

#include "../common/include/CommonConfig.h"

enum Video {
						NTSC,
						PAL,
						MPAL,
};

enum Sound {			Monoral,
						Stereo,
						Surround
};

enum AspectRatio {		_4to3,
						_16to9
};

enum GeneralONOFF {		Off,
						On
};

enum SystemLanguage {	Japanese,
						English,
						German,
						French,
						Spanish,
						Italian,
						Dutch,
						ChineseSimple,
						ChineseTraditional,
						Korean
};

void ConfigSyncBeforeRead( void );
void ConfigInit( void );

// BI2.bin region code.
extern u32 BI2region;

// Nintendont configuration.
static NIN_CFG *const ncfg = (NIN_CFG*)0x13002900;

// NOTE: DIChangeDisc() modifies this path.
static inline char *ConfigGetGamePath(void)
{
	return ncfg->GamePath;
}

static inline const char *ConfigGetCheatPath(void)
{
	return ncfg->CheatPath;
}

static inline bool ConfigGetConfig(u32 Config)
{
	return !!(ncfg->Config&Config);
}

static inline u32 ConfigGetVideoMode(void)
{
	return ncfg->VideoMode;
}

static inline u32 ConfigGetLanguage(void)
{
	return ncfg->Language;
}

static inline u32 ConfigGetMaxPads(void)
{
	return ncfg->MaxPads;
}

static inline u32 ConfigGetGameID(void)
{
	return ncfg->GameID;
}

static inline s8 ConfigGetVideoScale(void)
{
	return ncfg->VideoScale;
}

static inline s8 ConfigGetVideoOffset(void)
{
	return ncfg->VideoOffset;
}

static inline u32 ConfigGetMemcardCode(void)
{
	return MEM_CARD_CODE(ncfg->MemCardBlocks);
}

static inline u32 ConfigGetMemcardSize(void)
{
	return MEM_CARD_SIZE(ncfg->MemCardBlocks);
}

static inline void ConfigSetMemcardBlocks(u32 MemCardBlocks)
{
	ncfg->MemCardBlocks = MemCardBlocks;
	sync_after_write(&(ncfg->MemCardBlocks), sizeof(ncfg->MemCardBlocks));
}

#endif
