#include "Config.h"
#include "ff.h"
#include "debug.h"

NIN_CFG *ncfg = (NIN_CFG*)0x13002900;

void ConfigInit( void )
{
	FIL cfg;
	u32 read;

	dbgprintf("CFGInit()\r\n");
	sync_before_read(ncfg, sizeof(NIN_CFG));
	if (ncfg->Magicbytes != 0x01070CF6)
	{
		dbgprintf("Cfg not in memory, trying file\r\n");
		if (f_open(&cfg, "/nincfg.bin", FA_OPEN_EXISTING | FA_READ) != FR_OK)
		{
			dbgprintf("CFG:Failed to open config\r\n");
			Shutdown();
		}

		f_read( &cfg, ncfg, sizeof(NIN_CFG), &read );
		sync_after_write(ncfg, sizeof(NIN_CFG));
		f_close( &cfg );

		if( read != sizeof(NIN_CFG) )
		{
			dbgprintf("CFG:Failed to read config\r\n");
			Shutdown();
		}
	}

	if( IsWiiU )
	{
		ncfg->Config |= NIN_CFG_HID;
		ncfg->MaxPads = 0;
	}

	if( (read32(0) >> 8) == 0x47504F )	// PSO 1&2 disable cheats/debugging
	{
		ncfg->Config &= ~(NIN_CFG_CHEATS|NIN_CFG_DEBUGGER|NIN_CFG_DEBUGWAIT);
	}
}
inline char *ConfigGetGamePath( void )
{
	return ncfg->GamePath;
}
inline char *ConfigGetCheatPath( void )
{
	return ncfg->CheatPath;
}
inline bool ConfigGetConfig( u32 Config )
{
	return !!(ncfg->Config&Config);
}
inline u32 ConfigGetVideoMode( void )
{
	return ncfg->VideoMode;
}
inline u32 ConfigGetLanguage( void )
{
	return ncfg->Language;
}
inline u32 ConfigGetMaxPads(void)
{
	return ncfg->MaxPads;
}
inline u32 ConfigGetGameID(void)
{
	return ncfg->GameID;
}
