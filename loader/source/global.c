/*

Nintendont (Loader) - Playing Gamecubes in Wii mode on a Wii U

Copyright (C) 2013  crediar

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
#include <ogc/audio.h>
#include <ogc/consol.h>
#include <ogc/dsp.h>
#include <ogc/es.h>
#include <ogc/gx_struct.h>
#include <ogc/lwp_threads.h>
#include <ogc/system.h>
#include <ogc/video.h>
#include <ogc/wiilaunch.h>
#include <stdio.h>
#include <unistd.h>

#include "ff_utf8.h"
#include "diskio.h"

#include "Config.h"
#include "exi.h"
#include "global.h"
#include "font_zip.h"
#include "dip.h"
#include "unzip/unzip.h"

// Background image.
#include "background_png.h"

GRRLIB_ttfFont *myFont;
GRRLIB_texImg *background;
GRRLIB_texImg *screen_buffer;
static bool bg_isWidescreen = false;
static float bg_xScale = 1.0f;
static int bg_xPos = 0;

u32 POffset;

NIN_CFG* ncfg = (NIN_CFG*)0x93004000;
bool UseSD;

const char* const GetRootDevice()
{
	static const char* const SdStr = "sd";
	static const char* const UsbStr = "usb";
	if (UseSD)
		return SdStr;
	else
		return UsbStr;
}
void RAMInit(void)
{
	__asm("lis 3, 0x8390\n\
		  mtspr 0x3F3, 3");

	__asm("mfspr 3, 1008");
	__asm("ori 3, 3, 0x200");
	__asm("mtspr 1008, 3");

	__asm("mtfsb1    4*cr7+gt");

	memset( (void*)0x80000000, 0, 0x100 );
	memset( (void*)0x80003000, 0, 0x100 );
	//memset( (void*)0x80003F00, 0, 0x11FC100 );
	memset( (void*)0x81340000, 0, 0x3C );

	__asm("	isync\n\
	li      4, 0 \n\
	mtspr   541, 4\n\
	mtspr   540, 4 \n\
	mtspr   543, 4\n\
	mtspr   542, 4\n\
	mtspr   531, 4\n\
	mtspr   530, 4\n\
	mtspr   533, 4\n\
	mtspr   532, 4\n\
	mtspr   535, 4\n\
	mtspr   534, 4\n\
	isync");
	
	*(vu32*)0x80000028 = 0x01800000;
	*(vu32*)0x8000002C = 0;

	*(vu32*)0x8000002C = *(vu32*)0xCC00302C >> 28;		// console type
	*(vu32*)0x80000038 = 0x01800000;
	*(vu32*)0x800000F0 = 0x01800000;
	*(vu32*)0x800000EC = 0x81800000;
	
	*(vu32*)0x80003100 = 0x01800000;	//Physical MEM1 size 
	*(vu32*)0x80003104 = 0x01800000;	//Simulated MEM1 size 
	*(vu32*)0x80003108 = 0x01800000;
	*(vu32*)0x8000310C = 0;
	*(vu32*)0x80003110 = 0;
	*(vu32*)0x80003114 = 0;
	*(vu32*)0x80003118 = 0;				//Physical MEM2 size 
	*(vu32*)0x8000311C = 0;				//Simulated MEM2 size 
	*(vu32*)0x80003120 = 0;
	*(vu32*)0x80003124 = 0x0000FFFF;
	*(vu32*)0x80003128 = 0;
	*(vu32*)0x80003130 = 0x0000FFFF;	//IOS Heap Range 
	*(vu32*)0x80003134 = 0;
	*(vu32*)0x80003138 = 0x11;			//Hollywood Version 
	*(vu32*)0x8000313C = 0;

	*(vu32*)0x800030CC = 0;
	*(vu32*)0x800030C8 = 0;
	*(vu32*)0x800030D0 = 0;
	*(vu32*)0x800030C4 = 0;
	*(vu32*)0x800030C8 = 0;
	*(vu32*)0x800030D8 = 0;	
	*(vu32*)0x8000315C = 0x81;

//	__asm("lis 3, 0x8000\n");

	*(vu16*)0xCC00501A = 156;

	//*(vu32*)0xCC006400 = 0x340;

	*(vu32*)0x800030CC = 0;
	*(vu32*)0x800030C8 = 0;
	*(vu32*)0x800030D0 = 0;

//	memset( (void*)0x80003040, 0, 0x80 );

	*(vu32*)0x800030C4 = 0;
	*(vu32*)0x800030C8 = 0;

	//*(vu32*)0xCC003004 = 0xF0;
	//*(vu32*)0xCD000034 = 0x4000;

	*(vu32*)0x800030D8 = 0;
	
	*(vu32*)0x8000315C = 0x81;
}

void unzip_data(const void *input, const unsigned int input_size, 
	void **output, unsigned int *output_size)
{
	char filepath[20]; //statically linked zip file
	snprintf(filepath,20,"%x+%x",(unsigned int)input,input_size);
	unzFile uf = unzOpen(filepath); //opens zip in memory
	unzOpenCurrentFile(uf); //current file is the only file
	unz_file_info file_info; //get file info for uncompressed size
	unzGetCurrentFileInfo(uf,&file_info,NULL,0,NULL,0,NULL,0);
	*output_size = file_info.uncompressed_size; //set output size
	*output = malloc(*output_size); //allocate the required size
	unzReadCurrentFile(uf,*output,*output_size); //read it all
	unzCloseCurrentFile(uf); //done reading out the only file
	unzClose(uf); //close our static archive, all done!
}

static void *font_ttf = NULL;
static unsigned int font_ttf_size = 0;

/**
 * Initialize the loader.
 * This also loads the background image.
 * @param autoboot Set if autobooting. (This disables the fade-in.)
 */
void Initialise(bool autoboot)
{
	int i;
	AUDIO_Init(NULL);
	DSP_Init();
	AUDIO_StopDMA();
	AUDIO_RegisterDMACallback(NULL);
	CheckForGecko();
	gprintf("GRRLIB_Init = %i\r\n", GRRLIB_Init());
	unzip_data(font_zip, font_zip_size, &font_ttf, &font_ttf_size);
	gprintf("Decompressed font.ttf with %i bytes\r\n", font_ttf_size);
	myFont = GRRLIB_LoadTTF(font_ttf, font_ttf_size);
	background = GRRLIB_LoadTexturePNG(background_png);
	screen_buffer = GRRLIB_CreateEmptyTexture(rmode->fbWidth, rmode->efbHeight);

	// Calculate the background image scale.
	bg_isWidescreen = (CONF_GetAspectRatio() == CONF_ASPECT_16_9);
	if (bg_isWidescreen)
	{
		// Widescreen. 0.75x scaling, 80px offset.
		bg_xScale = 0.75f;
		bg_xPos = 80;
	}
	else
	{
		// Standard screen. 1.0x scaling, 0px offset.
		bg_xScale = 1.0f;
		bg_xPos = 0;
	}

	if(autoboot == false)
	{
		for (i=0; i<255; i +=5) // Fade background image in from black screen
		{
			if (bg_isWidescreen)
			{
				// Clear the sides.
				GRRLIB_Rectangle(0, 0, 80, 480, RGBA(222, 223, 224, i), true);
				GRRLIB_Rectangle(80+480, 0, 80, 480, RGBA(222, 223, 224, i), true);
			}
			GRRLIB_DrawImg(bg_xPos, 0, background, 0, bg_xScale, 1, RGBA(255, 255, 255, i)); // Opacity increases as i does
			GRRLIB_Render();
		}
		ClearScreen();
	}
	gprintf("Initialize Finished\r\n");
}

static void (*stub)() = (void*)0x80001800;
inline void DrawBuffer(void) {
	GRRLIB_DrawImg(0, 0, screen_buffer, 0, 1, 1, 0xFFFFFFFF);
}

inline void UpdateScreen(void) {
	GRRLIB_Screen2Texture(0, 0, screen_buffer, GX_FALSE);
	GRRLIB_Render();
	DrawBuffer();
}

raw_irq_handler_t BeforeIOSReload()
{
	__STM_Close();

	write32(0x80003140, 0);
	__MaskIrq(IRQ_PI_ACR);
	return IRQ_Free(IRQ_PI_ACR);
}
extern void udelay(u32 us);
void AfterIOSReload(raw_irq_handler_t handle, u32 rev)
{
	while((read32(0x80003140)) != rev)
		udelay(1000);

	u32 counter;
	for (counter = 0; !(read32(0x0d000004) & 2); counter++)
	{
		udelay(1000);
		if (counter >= 40000)
			break;
	}
	IRQ_Request(IRQ_PI_ACR, handle, NULL);
	__UnmaskIrq(IRQ_PI_ACR);
	__IPC_Reinitialize();

	__STM_Init();
}

/**
 * Exit Nintendont and return to the loader.
 * @param ret Exit code.
 */
#define SYSTEM_MENU			0x0000000100000002ULL
void ExitToLoader(int ret)
{
	extern vu32 KernelLoaded;

	UpdateScreen();
	UpdateScreen(); // Triple render to ensure it gets seen
	GRRLIB_Render();
	gprintf("Exiting Nintendont...\r\n");
	sleep(3);
	GRRLIB_FreeTexture(background);
	GRRLIB_FreeTexture(screen_buffer);
	GRRLIB_FreeTTF(myFont);
	GRRLIB_Exit();
	CloseDevices();
	if(KernelLoaded)
	{
		*(vu32*)0xD3003420 = 0x1DEA; //Kernel Reset
		while(*(vu32*)0xD3003420 != 0) usleep(20000);
	}
	gprintf("Loader Exit\n");
	VIDEO_SetBlack(TRUE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	//This whole exit routine is basically equal to the game exit stub
	if(*(vu32*)0x80001804 == 0x53545542 && *(vu32*)0x80001808 == 0x48415858) //stubhaxx
		__lwp_thread_stopmultitasking(stub);
	__ES_Init(); //make sure this is back open
	u32 numviews;
	STACK_ALIGN(tikview,views,4,32);
	ES_GetNumTicketViews(SYSTEM_MENU, &numviews);
	ES_GetTicketViews(SYSTEM_MENU, views, numviews);
	ES_LaunchTitle(SYSTEM_MENU, &views[0]);
	while(1) usleep(20000);
}

void LoaderShutdown()
{
	GRRLIB_FreeTexture(background);
	GRRLIB_FreeTexture(screen_buffer);
	GRRLIB_FreeTTF(myFont);
	GRRLIB_Exit();
	CloseDevices();
	gprintf("Loader Shutdown\n");
	VIDEO_SetBlack(TRUE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	SYS_ResetSystem(SYS_POWEROFF_STANDBY, 0, 0);
	while(1) usleep(20000);
}

/**
 * Load the configuration file from the root device.
 * @return True if loaded successfully; false if not.
 */
bool LoadNinCFG(void)
{
	bool ConfigLoaded = true;
	FIL cfg;
	if (f_open_char(&cfg, "/nincfg.bin", FA_READ|FA_OPEN_EXISTING) != FR_OK)
		return false;

	// Read the configuration file into memory.
	UINT BytesRead;
	f_read(&cfg, ncfg, sizeof(NIN_CFG), &BytesRead);
	f_close(&cfg);

	switch( ncfg->Version ) {
		case 2:
			if (BytesRead != 540)
				ConfigLoaded = false;
			break;

		default:
			if (BytesRead != sizeof(NIN_CFG))
				ConfigLoaded = false;
			break;
	}

	if (ncfg->Magicbytes != 0x01070CF6)
		ConfigLoaded = false;

	UpdateNinCFG();

	if (ncfg->Version != NIN_CFG_VERSION)
		ConfigLoaded = false;

	if (ncfg->MaxPads > NIN_CFG_MAXPAD)
		ConfigLoaded = false;

	return ConfigLoaded;
}

inline void ClearScreen()
{
	if (bg_isWidescreen)
	{
		// Clear the sides.
		GRRLIB_Rectangle(0, 0, 80, 480, RGBA(222, 223, 224, 255), true);
		GRRLIB_Rectangle(80+480, 0, 80, 480, RGBA(222, 223, 224, 255), true);
	}
	GRRLIB_DrawImg(bg_xPos, 0, background, 0, bg_xScale, 1, RGBA(255, 255, 255, 255));
}

static inline char ascii(char s)
{
	if (s < 0x20) return '.';
	else if (s > 0x7E) return '.';
	return s;
}

void hexdump(void *d, int len)
{
	if( d == (void*)NULL )
		return;

	u8 *data;
	int i, off;
	data = (u8*)d;

	for (off=0; off<len; off += 16)
	{
		gprintf("%08x  ",off);

		for(i=0; i<16; i++)
			if((i+off)>=len)
				gprintf("   ");
			else
				gprintf("%02x ",data[off+i]);

		gprintf(" ");

		for(i=0; i<16; i++)
			if((i+off)>=len)
				gprintf(" ");
			else
				gprintf("%c",ascii(data[off+i]));

		gprintf("\r\n");
	}
}
bool IsGCGame(u8 *Buffer)
{
	u32 AMB1 = *(vu32*)(Buffer+0x4);
	u32 GCMagic = *(vu32*)(Buffer+0x1C);
	return (AMB1 == 0x414D4231 || GCMagic == 0xC2339F3D);
}

void UpdateNinCFG()
{
	if (ncfg->Version == 2)
	{	//251 blocks, used to be there
		ncfg->NetworkProfile = 0x2;
		ncfg->Version = 3;
	}
	if (ncfg->Version == 3)
	{	//new memcard setting space
		ncfg->MemCardBlocks = ncfg->NetworkProfile;
		ncfg->VideoScale = 0;
		ncfg->VideoOffset = 0;
		ncfg->Version = 4;
	}
	if (ncfg->Version == 4)
	{	//Option got changed so not confuse it
		ncfg->Config &= ~NIN_CFG_HID;
		ncfg->Version = 5;
	}
	if (ncfg->Version == 5)
	{	//New Video Mode option
		ncfg->VideoMode &= ~NIN_VID_PATCH_PAL50;
		ncfg->Version = 6;
	}
	if (ncfg->Version == 6)
	{	//New flag, disabled by default
		ncfg->Config &= ~NIN_CFG_ARCADE_MODE;
		ncfg->Version = 7;
	}
	if (ncfg->Version == 7)
	{	// Wiimote CC Rumble, disabled by default;
		// don't skip IPL by default.
		ncfg->Config &= ~NIN_CFG_CC_RUMBLE;
		ncfg->Config &= ~NIN_CFG_SKIP_IPL;
		ncfg->Version = 8;
	}
	if (ncfg->Version == 8)
	{	// BBA Emu disabled by default
		// BBA Config 0 by default
		ncfg->Config &= ~NIN_CFG_BBA_EMU;
		ncfg->NetworkProfile = 0;
		ncfg->Version = 9;
	}
	if (ncfg->Version == 9)
	{
		ncfg->WiiUGamepadSlot = 0;
		ncfg->Version = 10;
	}
	if (ncfg->Version == 10)
	{
		// NIN_CFG with version 10 may have loaded
		// garbage bytes into WiiUGamepadSlot so sanitize
		// the slot if necessary.
		if (ncfg->WiiUGamepadSlot > NIN_CFG_MAXPAD)
		{
			ncfg->WiiUGamepadSlot = 0;
		}
	}
}

int CreateNewFile(const char *Path, unsigned int size)
{
	FIL f;

	// Check if the file already exists.
	if (f_open_char(&f, Path, FA_READ|FA_OPEN_EXISTING) == FR_OK)
	{	//create ONLY new files
		f_close(&f);
		return -1;
	}

	// Allocate a temporary buffer.
	void *buf = calloc(size, 1);
	if(buf == NULL)
	{
		gprintf("Failed to allocate %u bytes!\r\n", size);
		return -2;
	}

	// Create the file.
	if (f_open_char(&f, Path, FA_WRITE|FA_CREATE_NEW) != FR_OK)
	{
		gprintf("Failed to create %s!\r\n", Path);
		free(buf);
		return -3;
	}

	// Reserve space in the file.
	f_expand(&f, size, 1);

	// Write the temporary buffer to disk.
	UINT wrote;
	f_write(&f, buf, size, &wrote);
	f_close(&f);
	FlushDevices();
	free(buf);
	gprintf("Created %s with %u bytes!\r\n", Path, wrote);
	return 0;
}

/** Device mount/unmount. **/
// 0 == SD, 1 == USB
FATFS *devices[2];

// Device initialization data.
typedef struct _devInitInfo_t
{
	const WCHAR devNameFF[8];
	const char devNameDisplay[4];

	// Maximum init timeout, in seconds.
	// (0 = only try once)
	time_t timeout;
} devInitInfo_t;

static const devInitInfo_t devInitInfo[2] =
{
	{{'s', 'd', ':', 0}, "SD", 0},
	{{'u', 's', 'b', ':', 0}, "USB", 10}
};

/**
 * Initialize and mount a device.
 * @param pdrv Device number.
 * @return Mount point (WCHAR), or NULL on error.
 */
const WCHAR *MountDevice(BYTE pdrv)
{
	if (/*pdrv < DEV_SD ||*/ pdrv > DEV_USB)
		return NULL;

	// Attempt to initialize this device
	// TODO: Do initialization asynchronously.
	if (devInitInfo[pdrv].timeout > 0)
	{
		// Attempt multiple inits within a timeout period.
		time_t timeout = time(NULL);
		while (time(NULL) - timeout < devInitInfo[pdrv].timeout)
		{
			if (disk_initialize(pdrv) == 0)
				break;
			usleep(50000);
		}
	}
	else
	{
		// Only attempt a single init.
		disk_initialize(pdrv);
	}

	if (disk_status(pdrv) == 0)
	{
		// Device initialized.
		devices[pdrv] = (FATFS*)memalign(32, sizeof(FATFS));
		if (f_mount(devices[pdrv], devInitInfo[pdrv].devNameFF, 1) == FR_OK)
		{
			gprintf("Mounted %s!\n", devInitInfo[pdrv].devNameDisplay);
		}
		else
		{
			// Could not mount the filesystem.
			free(devices[pdrv]);
			devices[pdrv] = NULL;
		}
	}

	return (devices[pdrv] ? devInitInfo[pdrv].devNameFF : NULL);
}

/**
 * Unmount and shut down a device.
 * @param pdrv Device number.
 * @return 0 on success or if the drive was already shut down; non-zero on error.
 */
int UnmountDevice(BYTE pdrv)
{
	if (/*pdrv < DEV_SD ||*/ pdrv > DEV_USB)
		return -1;

	// FIXME: Close the log file if it's on this device?

	// Check if the device is mounted.
	if (devices[pdrv] != NULL)
	{
		// Unmount the device.
		f_mount(NULL, devInitInfo[pdrv].devNameFF, 1);
		// Free the FatFS object.
		free(devices[pdrv]);
		devices[pdrv] = 0;
	}

	// Shut down the device driver.
	disk_shutdown(pdrv);
	return 0;
}

/**
 * Shut down all devices.
 * This also closes the log file.
 */
void CloseDevices(void)
{
	closeLog();
	UnmountDevice(DEV_SD);
	UnmountDevice(DEV_USB);
}

/**
 * Flush all device caches.
 */
void FlushDevices(void)
{
	disk_flush(DEV_SD);
	disk_flush(DEV_USB);
}

/**
 * Does a filename have a supported file extension?
 * @return True if it does; false if it doesn't.
 */
bool IsSupportedFileExt(const char *filename)
{
	size_t len = strlen(filename);
	if (len >= 5 && filename[len-4] == '.')
	{
		const int extpos = len-3;
		if (!strcasecmp(&filename[extpos], "gcm") ||
		    !strcasecmp(&filename[extpos], "iso") ||
		    !strcasecmp(&filename[extpos], "cso"))
		{
			// File extension is supported.
			return true;
		}
	}
	else if (len >= 6 && filename[len-5] == '.')
	{
		const int extpos = len-4;
		if (!strcasecmp(&filename[extpos], "ciso"))
		{
			// File extension is supported.
			return true;
		}
	}

	// File extension is NOT supported.
	return false;
}

/**
 * Check if an ID6 is a known multi-game disc.
 * @param id6 ID6. (must be 6 bytes)
 * @return True if this is a known multi-game disc; false if not.
 */
bool IsMultiGameDisc(const char *id6)
{
	// Reference: https://gbatemp.net/threads/wit-wiimms-iso-tools-gamecube-disc-support.251630/#post-3088119
	// NOTE: GCOx52 is "Call of Duty: Finest Hour".
	if (!memcmp(id6, "GCO", 3) && id6[4]=='D' && id6[5]=='V')
	{
		// GCOxDV(D5) or GCOxDV(D9).
		return true;
	}

	// Check for older IDs.
	static const char multi_game_ids[3][8] = {"COBRAM", "GGCOSD", "RGCOSD"};
	u32 i;
	for (i = 0; i < 3; i++)
	{
		if (!memcmp(id6, multi_game_ids[i], 6))
		{
			// Found a multi-game disc.
			return true;
		}
	}

	// Not a multi-game disc.
	return false;
}
