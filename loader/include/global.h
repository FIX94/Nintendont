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
#include "Config.h"

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

#define MENU_POS_X			10
#define MENU_POS_Y			34

#define		HW_REG_BASE		0xCD800000
#define		HW_RESETS		(HW_REG_BASE + 0x194)

#define ALIGNED(x) __attribute__((aligned(x)))

extern u32 HollywoodRevision;
extern bool UseSD;
extern u32 Region;
extern u32 POffset;
extern NIN_CFG *ncfg;
extern FILE *cfg;

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

bool IsWiiU( void );
const char* const GetRootDevice();
void RAMInit(void);
void *Initialise();
bool LoadNinCFG();
void ExitToLoader(int ret);
void ClearScreen();
void CloseDevices();
void hexdump(void *d, int len);
void *memalign( u32 Align, u32 Size );

#endif

// 78A94

