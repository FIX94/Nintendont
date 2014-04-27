
#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "global.h"
#include "string.h"
#include "syscalls.h"
#include "global.h"
#include "ipc.h"
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

void ConfigInit( void );
char *ConfigGetGamePath( void );
char *ConfigGetCheatPath( void );
bool ConfigGetConfig( u32 Config );
u32 ConfigGetVideoMode( void );
u32 ConfigGetLanguage( void );
u32 ConfigGetMaxPads(void);
u32 ConfigGetGameID(void);
//bool IsWiiU;

#endif
