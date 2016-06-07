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

const unsigned char swi_v80[] =
{
	0xFF, 0xFF, 0x1F, 0x20,
};
const unsigned char swipatch_v80[] =
{
	0xFF, 0xFF, 0x7B, 0xD0,
};
const unsigned char EXISendBuffer[] =
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
const unsigned char UnusedSWI[] =
{
	0x46, 0x72,
	0x1C, 0x01,
	0x20, 0x05,
};

const unsigned char HWAccess_ESPatch[] =
{
    0x0D, 0x04, 0x00, 0x00,
    0x0D, 0x04, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x0F,
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x02,
};
const unsigned char HWAccess_ES[] =
{
    0x0D, 0x00, 0x00, 0x00,
	0x0D, 0x00, 0x00, 0x00,
	0x00, 0x0D, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x0F, 
    0x00, 0x00, 0x00, 0x01,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x02, 
} ;

static char Entry[0x1C] ALIGNED(32);

static char *Kernel = (char*)0x90100000;
u32 KernelSize = 0;

void InsertModule( char *Module, u32 ModuleSize )
{
	u32 loadersize = *(vu32*)(Kernel) + *(vu32*)(Kernel+4);
	u32 PatchCount = 0;
	int i = 0, j = 0;

#ifdef DEBUG_MODULE_PATCH
	gprintf("LoaderSize:%08X\r\n", loadersize );
#endif

	if( loadersize == 0 )
		return;

	if( loadersize > 0x1000 )
		return;

	Elf32_Ehdr *inhdr = (Elf32_Ehdr*)(Kernel+loadersize);

	unsigned int size = KernelSize;

	char *buf = Kernel;
	
	Elf32_Ehdr *outhdr = (Elf32_Ehdr *)(buf+loadersize);

	// set new ES thread entry point
	*(volatile unsigned int*)(buf+loadersize+0x254) = 0x20F00000;

	// Update ES stack address
	*(volatile unsigned int*)(buf+loadersize+0x264) = 0x2000; //size
	*(volatile unsigned int*)(buf+loadersize+0x26C) = 0x2010F000;

#ifdef DEBUG_MODULE_PATCH
	gprintf("PHeaders:%d\r\n", inhdr->e_phnum );
	gprintf("PHOffset:%d\r\n", inhdr->e_phoff );
#endif

	if( inhdr->e_phnum == 0 )
	{
#ifdef DEBUG_MODULE_PATCH
		gprintf("Error program header entries are zero!\r\n");
#endif
	} else {
	
		for( i=0; i < inhdr->e_phnum; ++i )
		{
			Elf32_Phdr *phdr = (Elf32_Phdr*)( Module + inhdr->e_phoff + sizeof( Elf32_Phdr ) * i );

			if( (phdr->p_vaddr & 0xF0000000) == 0x20000000  )
			{
#ifdef DEBUG_MODULE_PATCH
				gprintf("Type:%X Offset:%08X VAdr:%08X PAdr:%08X FSz:%08X MSz:%08X\r\n", (phdr->p_type), (phdr->p_offset), (phdr->p_vaddr), (phdr->p_paddr), (phdr->p_filesz), (phdr->p_memsz) );
#endif
				if( (phdr->p_filesz) > 100*1024*1024 )
					continue;

				//Look for entry
				for(j=0; j < (outhdr->e_phnum); ++j )
				{
					Elf32_Phdr *ophdr = (Elf32_Phdr *)( buf + loadersize + j*sizeof(Elf32_Phdr) + (outhdr->e_phoff) );
					
					if( (ophdr->p_vaddr) == 0x20100000 && (phdr->p_vaddr) == 0x20F00000 )
					{
#ifdef DEBUG_MODULE_PATCH
						gprintf("  O:Type:%X Offset:%08X VAdr:%08X PAdr:%08X FSz:%08X MSz:%08X\r\n", (ophdr->p_type), (ophdr->p_offset), (ophdr->p_vaddr), (ophdr->p_paddr), (ophdr->p_filesz), (ophdr->p_memsz) );
#endif
						ophdr->p_vaddr = phdr->p_vaddr;
						ophdr->p_paddr = phdr->p_paddr;
						
						ophdr->p_filesz = phdr->p_filesz;
						ophdr->p_memsz  = phdr->p_filesz;

						ophdr->p_offset = ( size - loadersize );
						size += (phdr->p_filesz);

						*(unsigned int*)(buf+8) = ( (*(unsigned int*)(buf+8)) + (phdr->p_filesz) );

						memcpy( buf+loadersize+(ophdr->p_offset), (char*)(Module+phdr->p_offset), phdr->p_filesz );
												
#ifdef DEBUG_MODULE_PATCH
						gprintf("  N:Type:%X Offset:%08X VAdr:%08X PAdr:%08X FSz:%08X MSz:%08X\r\n", (ophdr->p_type), (ophdr->p_offset), (ophdr->p_vaddr), (ophdr->p_paddr), (ophdr->p_filesz), (ophdr->p_memsz) );
#endif
						break;
					} else if( (ophdr->p_vaddr) == 0x2010D000 && (phdr->p_vaddr) == 0x20F38000 )
					{
#ifdef DEBUG_MODULE_PATCH
						gprintf("  O:Type:%X Offset:%08X VAdr:%08X PAdr:%08X FSz:%08X MSz:%08X\r\n", (ophdr->p_type), (ophdr->p_offset), (ophdr->p_vaddr), (ophdr->p_paddr), (ophdr->p_filesz), (ophdr->p_memsz) );
#endif
						ophdr->p_vaddr = phdr->p_vaddr;
						ophdr->p_paddr = phdr->p_paddr;
						// is not set correctly because of BSS, meaning the VMA isnt set up to the actually used point
						ophdr->p_filesz = 0x58000;
						ophdr->p_memsz  = 0x58000;

						ophdr->p_offset = ( size - loadersize );
						size += (phdr->p_filesz);

						*(unsigned int*)(buf+8) = ( (*(unsigned int*)(buf+8)) + (phdr->p_filesz) );

						memcpy( buf+loadersize+(ophdr->p_offset), (char*)(Module+phdr->p_offset), phdr->p_filesz );
						
#ifdef DEBUG_MODULE_PATCH
						gprintf("  N:Type:%X Offset:%08X VAdr:%08X PAdr:%08X FSz:%08X MSz:%08X\r\n", (ophdr->p_type), (ophdr->p_offset), (ophdr->p_vaddr), (ophdr->p_paddr), (ophdr->p_filesz), (ophdr->p_memsz) );
#endif
						break;
					}
					else if( (ophdr->p_vaddr) == 0x2010E000 && (phdr->p_vaddr) == 0x20106000 )
					{
#ifdef DEBUG_MODULE_PATCH
						gprintf("  O:Type:%X Offset:%08X VAdr:%08X PAdr:%08X FSz:%08X MSz:%08X\r\n", (ophdr->p_type), (ophdr->p_offset), (ophdr->p_vaddr), (ophdr->p_paddr), (ophdr->p_filesz), (ophdr->p_memsz) );
#endif
						ophdr->p_vaddr = phdr->p_vaddr;
						ophdr->p_paddr = phdr->p_paddr;
						
						ophdr->p_memsz = 0x9000;

						memcpy( buf+loadersize+(ophdr->p_offset), (char*)(Module+phdr->p_offset), phdr->p_filesz );
						
#ifdef DEBUG_MODULE_PATCH
						gprintf("  N:Type:%X Offset:%08X VAdr:%08X PAdr:%08X FSz:%08X MSz:%08X\r\n", (ophdr->p_type), (ophdr->p_offset), (ophdr->p_vaddr), (ophdr->p_paddr), (ophdr->p_filesz), (ophdr->p_memsz) );
#endif
						goto done;
					}
				}
			}
		}
	}

done:

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

		if( IsWiiU() )
		{
			if( PatchCount == 4 )
				break;
		} else {
			if( PatchCount == 7 )
				break;
		}
	}
	DCFlushRange(Kernel, KernelSize);
}
vu32 FoundVersion = 0;
s32 LoadKernel()
{
	u32 TMDSize;
	u32 i,u;

	s32 r = ES_GetStoredTMDSize( 0x000000010000003aLL, &TMDSize );
	if( r < 0 )
	{
		gprintf("ES_GetStoredTMDSize():%d\r\n", r );
		return r;
	}

	gprintf("TMDSize:%u\r\n", TMDSize );

	TitleMetaData *TMD = (TitleMetaData*)memalign( 32, TMDSize );
	if( TMD == (TitleMetaData*)NULL )
	{
		gprintf("Failed to alloc:%u\r\n", TMDSize );
		return r;	//todo errors are < 0 r still has >= 0 from previous call
	}

	r = ES_GetStoredTMD( 0x000000010000003aLL, (signed_blob *)TMD, TMDSize );
	if( r < 0 )
	{
		gprintf("ES_GetStoredTMD():%d\r\n", r );
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

	s32 cfd = IOS_Open( "/shared1/content.map", 1 );
	if( cfd < 0 )
	{
		gprintf("IOS_Open():%d\r\n", cfd );
		free(TMD);
		return cfd;
	}

	for( u=0;; u+=0x1C )
	{
		if( IOS_Read( cfd, Entry, 0x1C ) != 0x1C )
		{
			gprintf("Hash not found in content.map\r\n");
			free(TMD);
			return -2;
		}

		if( memcmp( (char*)(Entry+8), TMD->Contents[i].SHA1, 0x14 ) == 0 )
			break;
	}
	FoundVersion = ((TMD->TitleID & 0xFFFF) << 16) | (TMD->TitleVersion);
	free(TMD);

	IOS_Close( cfd );

	u32 *IOSVersion = (u32*)0x93003000;
	DCInvalidateRange(IOSVersion, 0x20);
	*IOSVersion = FoundVersion;
	gprintf("IOS Version: 0x%08X\n", FoundVersion);
	DCFlushRange(IOSVersion, 0x20);

	//char Path[32];
	char *Path = (char*)0x93003020;
	DCInvalidateRange(Path, 1024);
	memset(Path, 0, 1024);
	sprintf( Path, "/shared1/%.8s.app", Entry );
	gprintf("Kernel:\"%s\"\r\n", Path );
	DCFlushRange(Path, 1024);

	s32 kfd = IOS_Open( Path, 1 );
	if( kfd < 0 )
	{
		gprintf("IOS_Open():%d\r\n", kfd );
		return kfd;
	}

	KernelSize = IOS_Seek( kfd, 0, SEEK_END );
	IOS_Seek( kfd, 0, 0);

	gprintf("KernelSize:%u\r\n", KernelSize );

	if( IOS_Read( kfd, Kernel, KernelSize ) != KernelSize )
	{
		gprintf("IOS_Read() failed\r\n");

		IOS_Close(kfd);
		return -1;
	}
	IOS_Close(kfd);

	return 0;
}
