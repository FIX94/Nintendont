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
#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <gctypes.h>
#include <stdio.h>
#include <ogc/ipc.h>
#include "Config.h"
#include "grrlib.h"

#ifndef HW_RVL
#define HW_RVL
#endif

/*
 Add to CFLAGS in Makefile
*/
//#define DEBUG 1
//#define DEBUG_PATCHES
//#define DEBUG_MODULE_PATCH 1
//#define EXIPATCH
//#define SCREENSHOT

#define MENU_POS_X			25
#define MENU_POS_Y			34
#define DEFAULT_SIZE		16 // At 16, all characters are 10 pixels wide.  Use this to calculate centers
#define MENU_SIZE			16 // If we run out of screen space this should be made smaller

#define ARROW_LEFT			"\xE2\x97\x80"
#define ARROW_RIGHT			"\xE2\x96\xB6"

// RGBA Colors
#define AQUA				0x00FFFFFF
#define BLACK				0x000000FF
#define BLUE				0x0000FFFF
#define DARK_BLUE			0x00173bFF
#define FUCHSIA				0xFF00FFFF
#define GRAY				0x808080FF
#define GREEN				0x00C108FF
#define LIME				0x00FF00FF
#define MAROON				0x800000FF
#define NAVY				0x000080FF
#define OLIVE				0x808000FF
#define PURPLE				0x800080FF
#define RED					0xFF0000FF
#define SILVER				0xC0C0C0FF
#define TEAL				0x008080FF
#define WHITE				0xFFFFFFFF
#define YELLOW				0xFFFF00FF

#define		HW_REG_BASE		0xCD800000
#define		HW_RESETS		(HW_REG_BASE + 0x194)

#define ALIGNED(x) __attribute__((aligned(x)))
#define NORETURN __attribute__ ((noreturn))

#define ALIGN_FORWARD(x, align) \
	((typeof(x))(((x) + (typeof(x))(align) - 1) & (~((typeof(x))(align) - 1))))

#define ALIGN_BACKWARD(x, align) \
	((typeof(x))((x) & (~((typeof(x))(align) - 1))))

extern bool UseSD;
extern u32 POffset;
extern NIN_CFG *ncfg;
extern FILE *cfg;
extern GRRLIB_ttfFont *myFont;
extern GRRLIB_texImg *background;
extern GRRLIB_texImg *screen_buffer;

enum TRIGames
{
	TRI_NONE = 0,
	TRI_GP1,
	TRI_GP2,
	TRI_AX,
	TRI_VS4,
	TRI_SB,
};

enum ContentType
{
	CONTENT_REQUIRED=	(1<< 0),	// not sure
	CONTENT_SHARED	=	(1<<15),
	CONTENT_OPTIONAL=	(1<<14),
};

typedef struct
{
	u32 ID;				//	0	(0x1E4)
	u16 Index;			//	4	(0x1E8)
	u16 Type;			//	6	(0x1EA)
	u64 Size;			//	8	(0x1EC)
	u8	SHA1[20];		//  12	(0x1F4)
} __attribute__((packed)) Content;

typedef struct
{
	u32 SignatureType;		// 0x000
	u8	Signature[0x100];	// 0x004

	u8	Padding0[0x3C];		// 0x104
	u8	Issuer[0x40];		// 0x140

	u8	Version;			// 0x180
	u8	CACRLVersion;		// 0x181
	u8	SignerCRLVersion;	// 0x182
	u8	Padding1;			// 0x183

	u64	SystemVersion;		// 0x184
	u64	TitleID;			// 0x18C 
	u32	TitleType;			// 0x194 
	u16	GroupID;			// 0x198 
	u8	Reserved[62];		// 0x19A 
	u32	AccessRights;		// 0x1D8
	u16	TitleVersion;		// 0x1DC 
	u16	ContentCount;		// 0x1DE 
	u16 BootIndex;			// 0x1E0
	u8	Padding3[2];		// 0x1E2 

	Content Contents[];		// 0x1E4 

} __attribute__((packed)) TitleMetaData;

/**
 * Is this system a Wii U?
 * @return True if this is Wii U; false if not.
 */
extern bool isWiiVC;
static inline bool IsWiiU(void)
{
	return ((*(vu16*)0xCD8005A0 == 0xCAFE) || isWiiVC);
}
static inline bool IsWiiUFastCPU(void)
{
	return ((*(vu16*)0xCD8005A0 == 0xCAFE) && ((*(vu32*)0xCD8005B0 & 0x20) == 0));
}
// FIXME: This return type isn't quite correct...
const char* const GetRootDevice();
void RAMInit(void);
void Initialise(bool autoboot);
void unzip_data(const void *input, const unsigned int input_size, 
	void **output, unsigned int *output_size);

/**
 * Load the configuration file from the root device.
 * @return True if loaded successfully; false if not.
 */
bool LoadNinCFG(void);

void UpdateNinCFG();
bool IsGCGame(u8 *Buffer);
int CreateNewFile(const char *Path, unsigned int size);

/**
 * Exit Nintendont and return to the loader.
 * @param ret Exit code.
 */
void ExitToLoader(int ret) NORETURN;
void LoaderShutdown() NORETURN;

void ClearScreen();
void CloseDevices();
void hexdump(void *d, int len);
void DrawBuffer(void);
void UpdateScreen(void);
void Screenshot(void);
raw_irq_handler_t BeforeIOSReload();
void AfterIOSReload(raw_irq_handler_t handle, u32 rev);

/** Device mount/unmount. **/
#include "integer.h"	/* for WCHAR */
const WCHAR *MountDevice(BYTE pdrv);
int UnmountDevice(BYTE pdrv);
void CloseDevices(void);
void FlushDevices(void);

/**
 * Does a filename have a supported file extension?
 * @return True if it does; false if it doesn't.
 */
bool IsSupportedFileExt(const char *filename);

/**
 * Check if an ID6 is a known multi-game disc.
 * @param id6 ID6. (must be 6 bytes)
 * @return True if this is a known multi-game disc; false if not.
 */
bool IsMultiGameDisc(const char *id6);

#endif

// 78A94
