#include "Config.h"
#include "debug.h"
#include "ff_utf8.h"

// BI2.bin region code.
u32 BI2region = 0;

void ConfigSyncBeforeRead( void )
{
	sync_before_read(ncfg, sizeof(NIN_CFG));
}

void ConfigInit( void )
{
	FIL cfg;
	u32 read;

	dbgprintf("CFGInit()\r\n");
	ConfigSyncBeforeRead();
	if (ncfg->Magicbytes != 0x01070CF6)
	{
		dbgprintf("Cfg not in memory, trying file\r\n");
		if (f_open_char(&cfg, "/nincfg.bin", FA_OPEN_EXISTING | FA_READ) != FR_OK)
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
		ConfigSyncBeforeRead();
	}

	if( IsWiiU() )
	{
		//ncfg->Config |= NIN_CFG_HID;

		// Disable debugging and the drive access LED.
		ncfg->Config &= ~(NIN_CFG_DEBUGGER | NIN_CFG_DEBUGWAIT | NIN_CFG_LED);
	}

	//if( (read32(0) >> 8) == 0x47504F )	// PSO 1&2 disable cheats/debugging
	//{
	//	ncfg->Config &= ~(NIN_CFG_CHEATS|NIN_CFG_DEBUGGER|NIN_CFG_DEBUGWAIT);
	//}
}
