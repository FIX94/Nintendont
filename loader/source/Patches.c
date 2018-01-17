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
#include <ogc/machine/processor.h>
#include <ogc/cache.h>
#include <ogc/video.h>
#include <ogc/video_types.h>
#include <ogc/es.h>
#include <ogc/ipc.h>
#include <malloc.h>
#include "Patches.h"
#include "exi.h"
#include "Config.h"
#include "ELF.h"

// for printing Load_Kernel() errors.
#include "menu.h"

static const unsigned char swi_v80[] =
{
	0xFF, 0xFF, 0x1F, 0x20,
};
static const unsigned char swipatch_v80[] =
{
	0xFF, 0xFF, 0x7B, 0xD0,
};
static const unsigned char EXISendBuffer[] =
{
	0xE9, 0xAD, 0x40, 0x1F, 0xE1, 0x5E, 0x30, 0xB2, 0xE2, 0x03, 0x30, 0xFF, 0xE3, 0x53, 0x00, 0xAB, 
	0x1A, 0x00, 0x00, 0x07, 0xE3, 0x50, 0x00, 0x04, 0x1A, 0x00, 0x00, 0x05, 0xE5, 0x9F, 0x30, 0x58, 
	0xE5, 0xD1, 0x20, 0x00, 0xEB, 0x00, 0x00, 0x04, 0xE2, 0x81, 0x10, 0x01, 0xE3, 0x52, 0x00, 0x00, 
	0x1A, 0xFF, 0xFF, 0xFA, 0xE8, 0x3D, 0x40, 0x1F, 0xE1, 0xB0, 0xF0, 0x0E, 0xE3, 0xA0, 0x00, 0xD0, 
	0xE5, 0x83, 0x00, 0x00, 0xE3, 0xA0, 0x02, 0x0B, 0xE1, 0x80, 0x0A, 0x02, 0xE5, 0x83, 0x00, 0x10, 
	0xE3, 0xA0, 0x00, 0x19, 0xE5, 0x83, 0x00, 0x0C, 0xE5, 0x93, 0x00, 0x0C, 0xE3, 0x10, 0x00, 0x01, 
	0x1A, 0xFF, 0xFF, 0xFC, 0xE5, 0x93, 0x00, 0x10, 0xE3, 0x10, 0x03, 0x01, 0xE3, 0xA0, 0x00, 0x00, 
	0xE5, 0x83, 0x00, 0x00, 0x0A, 0xFF, 0xFF, 0xF0, 0xE1, 0xA0, 0xF0, 0x0E, 0x0D, 0x80, 0x68, 0x14, 
};
static const unsigned char UnusedSWI[] =
{
	0x46, 0x72,
	0x1C, 0x01,
	0x20, 0x05,
};

/* Full HW 0x0D8000000 Access in Nintendont */
static const unsigned char HWAccess_ES[] =
{
	0x0D, 0x80, 0x00, 0x00, //virt address (0x0D8000000)
	0x0D, 0x80, 0x00, 0x00, //phys address (0x0D8000000)
	0x00, 0x0D, 0x00, 0x00, //length (up to 0x0D8D00000)
	0x00, 0x00, 0x00, 0x0F, 
	0x00, 0x00, 0x00, 0x02, //permission (2=ro, patchme)
	0x00, 0x00, 0x00, 0x00,
};
static const unsigned char HWAccess_ESPatch[] =
{
	0x0D, 0x80, 0x00, 0x00, //virt address (0x0D8000000)
	0x0D, 0x80, 0x00, 0x00, //phys address (0x0D8000000)
	0x00, 0x0D, 0x00, 0x00, //length (up to 0x0D8D00000)
	0x00, 0x00, 0x00, 0x0F,
	0x00, 0x00, 0x00, 0x03, //permission (3=rw)
	0x00, 0x00, 0x00, 0x00,
};

/* Replace ES Ioctl 0x1F to start Nintendont */
static const unsigned char ES_Ioctl_1F[] =
{
	0x68, 0x4B, 0x2B, 0x06, 0xD1, 0x0C, 0x68, 0x8B, 0x2B, 0x00, 0xD1, 0x09, 0x68, 0xC8, 0x68, 0x42,
	0x23, 0xA9, 0x00, 0x9B
};
static const unsigned char ES_Ioctl_1F_Patch[] =
{
	0x49, 0x01, 0x47, 0x88, 0x46, 0xC0, 0xE0, 0x01, 0x12, 0xFF, 0xFE, 0x00, 0x22, 0x00, 0x23, 0x01, 
	0x46, 0xC0, 0x46, 0xC0
};

/* Since ES Ioctl 0x1F is replaced its function can be used */
static const unsigned char ES_Ioctl_1F_Function[] =
{
	0xB5, 0xF0, 0x46, 0x5F, 0x46, 0x56, 0x46, 0x4D, 0x46, 0x44, 0xB4, 0xF0, 0xB0, 0xBA, 0x1C, 0x0F
};
static const unsigned char ES_Ioctl_1F_Function_Patch[] = {
	0xE5, 0x9F, 0x10, 0x04, 0xE5, 0x91, 0x00, 0x00, 0xE1, 0x2F, 0xFF, 0x10, 0x12, 0xFF, 0xFF, 0xE0
};

static char Entry[0x1C] ALIGNED(32);

// IOS58 kernel memory base address.
static char *const KernelDst = (char*)0x90100000;
// kernel address we first read into
static char *const KernelReadBuf = (char*)0x91000000;
static unsigned int KernelSize = 0;

void PatchKernel()
{
	unsigned int loadersize = *(vu32*)(KernelReadBuf) + *(vu32*)(KernelReadBuf+4);
	u32 PatchCount = 0;
	int i = 0;

#ifdef DEBUG_MODULE_PATCH
	gprintf("LoaderSize:%08X\r\n", loadersize );
#endif

	if( loadersize == 0 )
		return;

	if( loadersize > 0x1000 )
		return;

	unsigned int size = KernelSize;

	char *buf = KernelReadBuf;

	for( i=0; i < size; i+=4 )
	{
		if( !IsWiiU() )
		{
			if( memcmp( buf+i, UnusedSWI, sizeof(UnusedSWI) ) == 0 )
			{
#ifdef DEBUG_MODULE_PATCH
				gprintf("Found Unused SWI at %08X\r\n", i );
#endif
				memcpy( buf+i, EXISendBuffer, sizeof( EXISendBuffer ) );
				PatchCount |= 1;
			}

			if( memcmp( buf+i, swi_v80, sizeof(swi_v80) ) == 0 )
			{
#ifdef DEBUG_MODULE_PATCH
				gprintf("Found SWI at %08X\r\n", i );
#endif
				memcpy( buf+i, swipatch_v80, sizeof( swipatch_v80 ) );
				PatchCount |= 2;
			}
		}

		if( memcmp( buf+i, HWAccess_ES, sizeof(HWAccess_ES) ) == 0 )
		{
#ifdef DEBUG_MODULE_PATCH
			gprintf("Found HWAccess_ES at %08X\r\n", i );
#endif
			memcpy( buf+i, HWAccess_ESPatch, sizeof( HWAccess_ESPatch ) );
			PatchCount |= 4;
		}

		if( memcmp( buf+i, ES_Ioctl_1F, sizeof(ES_Ioctl_1F) ) == 0 )
		{
#ifdef DEBUG_MODULE_PATCH
			gprintf("Found ES Ioctl 0x1F at %08X\r\n", i );
#endif
			memcpy( buf+i, ES_Ioctl_1F_Patch, sizeof( ES_Ioctl_1F_Patch ) );
			PatchCount |= 8;
		}

		if( memcmp( buf+i, ES_Ioctl_1F_Function, sizeof(ES_Ioctl_1F_Function) ) == 0 )
		{
#ifdef DEBUG_MODULE_PATCH
			gprintf("Found ES Ioctl 0x1F Function at %08X\r\n", i );
#endif
			memcpy( buf+i, ES_Ioctl_1F_Function_Patch, sizeof( ES_Ioctl_1F_Function_Patch ) );
			PatchCount |= 0x10;
		}

		if( IsWiiU() )
		{
			if( PatchCount == 0x1C )
				break;
		} else {
			if( PatchCount == 0x1F )
				break;
		}
	}
	//copy into place AFTER patching
	memcpy(KernelDst, KernelReadBuf, KernelSize);
	DCFlushRange(KernelDst, KernelSize);
}

// Title ID for IOS58.
static const u64 TitleID_IOS58 = 0x000000010000003AULL;

// Version number of the IOS we're using.
// (Major/minor, not IOS slot.)
volatile unsigned int FoundVersion = 0;

/**
 * Load and patch IOS.
 * @return 0 on success; negative on error.
 */
int LoadKernel(void)
{
	unsigned int TMDSize;
	unsigned int i,u;

	int r = ES_GetStoredTMDSize(TitleID_IOS58, (u32*)&TMDSize);
	if (r < 0)
	{
		// IOS58 not found.
		gprintf("ES_GetStoredTMDSize(0x%llX) failed: %d\r\n", TitleID_IOS58, r);
		PrintLoadKernelError(LKERR_ES_GetStoredTMDSize, r);
		return r;
	}

	gprintf("TMDSize:%u\r\n", TMDSize );

	TitleMetaData *TMD = (TitleMetaData*)memalign( 32, TMDSize );
	if(!TMD)
	{
		// Memory allocation failure.
		// NOTE: Not an IOS error, so we'll have to fake
		// a negative error code.
		gprintf("Failed to alloc: %u\r\n", TMDSize);
		PrintLoadKernelError(LKERR_TMD_malloc, -1);
		return -1;
	}

	r = ES_GetStoredTMD(TitleID_IOS58, (signed_blob*)TMD, TMDSize);
	if (r < 0)
	{
		// Unable to load IOS58.
		gprintf("ES_GetStoredTMD(0x%llX) failed: %d\r\n", TitleID_IOS58, r);
		PrintLoadKernelError(LKERR_ES_GetStoredTMD, r);
		free(TMD);
		return r;
	}

//Look for boot index
	for( i=0; i < TMD->ContentCount; ++i )
	{
		if( TMD->BootIndex == TMD->Contents[i].Index )
			break;
	}
	
	gprintf("BootIndex:%u\r\n", i );

	int cfd = IOS_Open("/shared1/content.map", 1);
	if (cfd < 0)
	{
		gprintf("IOS_Open(\"/shared1/content.map\") failed: %d\r\n", cfd);
		free(TMD);
		PrintLoadKernelError(LKERR_IOS_Open_shared1_content_map, cfd);
		return cfd;
	}

	for (u=0; ; u+=0x1C)
	{
		if (IOS_Read(cfd, Entry, 0x1C) != 0x1C)
		{
			gprintf("Hash not found in content.map\r\n");
			free(TMD);
			PrintLoadKernelError(LKERR_HashNotFound, -1);
			return -2;
		}

		if (memcmp((char*)(Entry+8), TMD->Contents[i].SHA1, 0x14) == 0)
			break;
	}
	FoundVersion = ((TMD->TitleID & 0xFFFF) << 16) | (TMD->TitleVersion);
	free(TMD);

	IOS_Close( cfd );

	u32 *const IOSVersion = (u32*)0x93003000;
	DCInvalidateRange(IOSVersion, 0x20);
	*IOSVersion = FoundVersion;
	gprintf("IOS Version: 0x%08X\n", FoundVersion);
	DCFlushRange(IOSVersion, 0x20);

	char Path[32];
	snprintf(Path, 32, "/shared1/%.8s.app", Entry);
	gprintf("Kernel:\"%s\"\r\n", Path );
	DCFlushRange(Path, 32);

	// Open the actual IOS58 kernel file.
	int kfd = IOS_Open(Path, 1);
	if (kfd < 0)
	{
		gprintf("IOS_Open(\"%s\") failed: %d\r\n", Path, kfd);
		PrintLoadKernelError(LKERR_IOS_Open_IOS58_kernel, kfd);
		return kfd;
	}

	KernelSize = IOS_Seek( kfd, 0, SEEK_END );
	IOS_Seek( kfd, 0, 0);

	gprintf("KernelSize:%u\r\n", KernelSize );
	DCInvalidateRange(KernelReadBuf, KernelSize);
	if (IOS_Read(kfd, KernelReadBuf, KernelSize) != KernelSize)
	{
		gprintf("IOS_Read() failed\r\n");
		PrintLoadKernelError(LKERR_IOS_Read_IOS58_kernel, kfd);
		IOS_Close(kfd);
		return -1;
	}

	IOS_Close(kfd);
	return 0;
}
