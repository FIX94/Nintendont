/*
ISO.c for Nintendont (Kernel)

Copyright (C) 2014 FIX94

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
#include "Config.h"
#include "ISO.h"
#include "FST.h"
#include "DI.h"
#include "EXI.h"
#include "GCNCard.h"
#include "debug.h"
#include "wdvd.h"

#include "ff_utf8.h"

extern u32 TRIGame;
extern u32 DiscRequested;
extern bool wiiVCInternal;

u32 ISOFileOpen = 0;

#define CACHE_MAX		0x400
#define CACHE_START		(u8*)0x11000000
#define CACHE_SIZE		0x1E80000

typedef struct
{
	u32 Offset;
	u32 Size;
	u8 *Data;
} DataCache;

static u32 CacheInited = 0;
static u32 TempCacheCount = 0;
static u32 DataCacheOffset = 0;
static u8 *DCCache = CACHE_START;
static u32 DCacheLimit = CACHE_SIZE;
static DataCache DC[CACHE_MAX];

extern u32 USBReadTimer;
static FIL GameFile;
static u64 LastOffset64 = ~0ULL;
bool Datel = false;

// CISO: On-disc structure.
// Temporarily loaded into cache memory.
#define CISO_MAGIC	0x4349534F /* "CISO" */
#define CISO_HEADER_SIZE 0x8000
#define CISO_MAP_SIZE_MAX (CISO_HEADER_SIZE - (sizeof(u32) * 2))
#define CISO_MAP_SIZE	1024
#define CISO_BLOCK_SIZE (2*1024*1024)
typedef struct _CISO_t {
	u32 magic;		// "CISO" (0x4349534F)
	u32 block_size;		// usually 2 MB (LE32)
	u8 map[CISO_MAP_SIZE];	// Block map;
} CISO_t;

// CISO: Block map.
// Supports files up to 2 GB when using 2 MB blocks.
static uint16_t ciso_block_map[CISO_MAP_SIZE];
static bool ISO_IsCISO = false;	// Set to 1 for CISO mode.

/**
 * Read directly from the ISO file.
 * CISO mapping is done here.
 * @param Buffer Output buffer.
 * @param Length Data length.
 * @param Offset ISO file offset. (Must have ISOShift64 added!)
 */
static inline void ISOReadDirect(void *Buffer, u32 Length, u64 Offset64)
{
	if(ISOFileOpen == 0)
		return;

	UINT read;
	if (!ISO_IsCISO)
	{
		// Standard ISO/GCM file.
		if(LastOffset64 != Offset64)
		{
			if(wiiVCInternal)
				WDVD_FST_LSeek( Offset64 );
			else
				f_lseek( &GameFile, Offset64 );
		}
		if(wiiVCInternal)
		{
			sync_before_read( Buffer, Length );
			read = WDVD_FST_Read( Buffer, Length );
		}
		else
			f_read( &GameFile, Buffer, Length, &read );
	}
	else
	{
		// CISO. Handle individual blocks.
		// TODO: LastOffset64 optimization?
		u8 *ptr8 = (u8*)Buffer;

		// Check if we're not starting on a block boundary.
		const u32 blockStartOffset = (u32)(Offset64 % CISO_BLOCK_SIZE);
		if (blockStartOffset != 0)
		{
			// Not a block boundary.
			// Read the end of the block.
			u32 read_sz = CISO_BLOCK_SIZE - blockStartOffset;
			if (Length < read_sz)
				read_sz = Length;

			// Get the physical block number first.
			const u32 blockStart = Offset64 / CISO_BLOCK_SIZE;
			if (blockStart >= CISO_MAP_SIZE)
			{
				// Out of range.
				return;
			}

			const u16 physBlockStartIdx = ciso_block_map[blockStart];
			if (physBlockStartIdx == 0xFFFF)
			{
				// Empty block.
				memset(ptr8, 0, read_sz);
			}
			else
			{
				// Seek to the physical block address.
				const u32 physBlockStartAddr = CISO_HEADER_SIZE + ((u32)physBlockStartIdx * CISO_BLOCK_SIZE);
				if(wiiVCInternal)
				{
					WDVD_FST_LSeek( physBlockStartAddr + blockStartOffset );
					sync_before_read( ptr8, read_sz );
					read = WDVD_FST_Read( ptr8, read_sz );
				}
				else
				{
					f_lseek(&GameFile, physBlockStartAddr + blockStartOffset);
					// Read read_sz bytes.
					f_read(&GameFile, ptr8, read_sz, &read);
				}
				if (read != read_sz)
				{
					// Error reading the data.
					return;
				}
			}

			// Starting block read.
			Length -= read_sz;
			ptr8 += read_sz;
			Offset64 += read_sz;
		}

		// Read entire blocks.
		for (; Length >= CISO_BLOCK_SIZE;
		    Length -= CISO_BLOCK_SIZE, ptr8 += CISO_BLOCK_SIZE,
		    Offset64 += CISO_BLOCK_SIZE)
		{
			const u32 blockIdx = (u32)(Offset64 / CISO_BLOCK_SIZE);
			if (blockIdx >= CISO_MAP_SIZE)
			{
				// Out of range.
				return;
			}

			const u16 physBlockIdx = ciso_block_map[blockIdx];
			if (physBlockIdx == 0xFFFF)
			{
				// Empty block.
				memset(ptr8, 0, CISO_BLOCK_SIZE);
			}
			else
			{
				// Seek to the physical block address.
				const u32 physBlockAddr = CISO_HEADER_SIZE + ((u32)physBlockIdx * CISO_BLOCK_SIZE);
				if(wiiVCInternal)
				{
					WDVD_FST_LSeek( physBlockAddr );
					sync_before_read( ptr8, CISO_BLOCK_SIZE );
					read = WDVD_FST_Read( ptr8, CISO_BLOCK_SIZE );
				}
				else
				{
					f_lseek(&GameFile, physBlockAddr);
					// Read one block worth of data.
					f_read(&GameFile, ptr8, CISO_BLOCK_SIZE, &read);
				}
				if (read != CISO_BLOCK_SIZE)
				{
					// Error reading the data.
					return;
				}
			}
		}

		// Check if we still have data left. (not a full block)
		if (Length > 0) {
			// Not a full block.

			// Get the physical block number first.
			const u32 blockEnd = (u32)(Offset64 / CISO_BLOCK_SIZE);
			if (blockEnd >= CISO_MAP_SIZE)
			{
				// Out of range.
				return;
			}

			const u16 physBlockEndIdx = ciso_block_map[blockEnd];
			if (physBlockEndIdx == 0xFFFF)
			{
				// Empty block.
				memset(ptr8, 0, Length);
			}
			else
			{
				// Seek to the physical block address.
				const u32 physBlockEndAddr = CISO_HEADER_SIZE + ((u32)physBlockEndIdx * CISO_BLOCK_SIZE);
				if(wiiVCInternal)
				{
					WDVD_FST_LSeek( physBlockEndAddr );
					sync_before_read( ptr8, Length );
					read = WDVD_FST_Read( ptr8, Length );
				}
				else
				{
					f_lseek(&GameFile, physBlockEndAddr);
					// Read Length bytes.
					f_read(&GameFile, ptr8, Length, &read);
				}
				if (read != Length)
				{
					// Error reading the data.
					return;
				}
			}

			Offset64 += Length;
		}
	}

	LastOffset64 = Offset64 + Length;
	//refresh read timeout
	USBReadTimer = read32(HW_TIMER);
}

// ISO shift offset.
extern u64 ISOShift64;
static u8 isoTmpBuf[0x20] ALIGNED(32);

bool ISOInit()
{
	if(wiiVCInternal)
	{
		if(WDVD_FST_OpenDisc(DiscRequested) != 0)
			Shutdown();
	}
	else
	{
		s32 ret = f_open_char( &GameFile, ConfigGetGamePath(), FA_READ|FA_OPEN_EXISTING );
		if( ret != FR_OK )
			return false;

#if _USE_FASTSEEK
		/* Setup table */
		u32 tblsize = 4; //minimum default size
		GameFile.cltbl = malloc(tblsize * sizeof(DWORD));
		GameFile.cltbl[0] = tblsize;
		ret = f_lseek(&GameFile, CREATE_LINKMAP);
		if( ret == FR_NOT_ENOUGH_CORE )
		{	/* We need more table mem */
			tblsize = GameFile.cltbl[0];
			free(GameFile.cltbl);
			dbgprintf("ISO:Fragmented, allocating %08x\r\n", tblsize);
			GameFile.cltbl = malloc(tblsize * sizeof(DWORD));
			GameFile.cltbl[0] = tblsize;
			f_lseek(&GameFile, CREATE_LINKMAP);
		}
#endif /* _USE_FASTSEEK */
	}

	/* Setup direct reader */
	ISOFileOpen = 1;
	LastOffset64 = ~0ULL;
	ISO_IsCISO = false;

	/* Check for CISO format. */
	CISO_t *tmp_ciso = (CISO_t*)malloca(0x8000, 0x20);
	ISOReadDirect(tmp_ciso, 0x8000, 0);
	if (tmp_ciso->magic == CISO_MAGIC)
	{
		// Only CISOs with 2 MB block sizes are supported.
		u32 block_size = ((tmp_ciso->block_size >> 24) & 0x000000FF) |
				 ((tmp_ciso->block_size >>  8) & 0x0000FF00) |
				 ((tmp_ciso->block_size <<  8) & 0x00FF0000) |
				 ((tmp_ciso->block_size << 24) & 0xFF000000);
		if (block_size == CISO_BLOCK_SIZE)
		{
			// CISO has 2 MB blocks.

			/**
			 * Initialize the CISO block map.
			 * Valid entries:
			 * - 0: Empty block.
			 * - 1: Used block.
			 *
			 * FIXME: Currently handling other values
			 * as "used" blocks. An invalid value
			 * should cause an error.
			 */
			uint16_t physBlockIdx = 0;
			int i;
			for (i = 0; i < CISO_MAP_SIZE; i++)
			{
				if (tmp_ciso->map[i])
				{
					// Used block.
					ciso_block_map[i] = physBlockIdx;
					physBlockIdx++;
				}
				else
				{
					// Empty block.
					ciso_block_map[i] = 0xFFFF;
				}
			}

			// Enable CISO mode.
			ISO_IsCISO = true;
		}
	}
	free(tmp_ciso);

	/* Set Low Mem */
	ISOReadDirect(isoTmpBuf, 0x20, 0x0 + ISOShift64);
	memcpy((void*)0x0, isoTmpBuf, 0x20);
	sync_after_write((void*)0x0, 0x20); //used by game so sync it
	/* Get BI2.bin region code */
	ISOReadDirect(isoTmpBuf, sizeof(BI2region), 0x458 + ISOShift64);
	memcpy(&BI2region, isoTmpBuf, sizeof(BI2region));
	/* Reset Cache */
	CacheInited = 0;

	if ((read32(0) == 0x474E4845) && (read32(4) == 0x35640000))
	{
		u32 DatelName[2];
		ISOReadDirect(isoTmpBuf, 2 * sizeof(u32), 0x19848 + ISOShift64);
		memcpy(DatelName, isoTmpBuf, 2 * sizeof(u32));
		if ((DatelName[0] == 0x20446174) && ((DatelName[1] >> 16) == 0x656C))
			Datel = true;
	}
	return true;
}

void ISOClose()
{
	if(ISOFileOpen)
	{
		if(wiiVCInternal)
			WDVD_FST_Close();
		else
		{
			f_close( &GameFile );
#if _USE_FASTSEEK
			free(GameFile.cltbl);
			GameFile.cltbl = NULL;
#endif /* _USE_FASTSEEK */
		}
	}
	ISOFileOpen = 0;
	ISO_IsCISO = false;
}

void ISOSetupCache()
{
	if(ISOFileOpen == 0 || CacheInited)
		return;

	DCCache = CACHE_START;
	DCacheLimit = CACHE_SIZE;
	/* Setup Caching */
	if(TRIGame)
	{
		//AMBB buffer is before cache
		DCCache += 0x10000;
		DCacheLimit -= 0x10000;
		//triforce buffer is after cache
		DCacheLimit -= 0x300000;
	}
	else
	{
		u32 MemCardSize = 0;
		if (ConfigGetConfig(NIN_CFG_MEMCARDEMU))
		{
			// Get the total card size from GCNCard.c.
			MemCardSize = GCNCard_GetTotalSize();
		}
		DCCache += MemCardSize; //memcard is before cache
		DCacheLimit -= MemCardSize;
	}
	memset32(DC, 0, sizeof(DataCache)* CACHE_MAX);

	DataCacheOffset = 0;
	TempCacheCount = 0;

	CacheInited = 1;
}

void ISOSeek(u32 Offset)
{
	if(ISOFileOpen == 0)
		return;

	const u64 Offset64 = (u64)Offset + ISOShift64;
	if(LastOffset64 != Offset64)
	{
		if (ISO_IsCISO)
		{
			const u32 blockIdx = (u32)(Offset64 / CISO_BLOCK_SIZE);
			if (blockIdx >= CISO_MAP_SIZE)
				return;

			const u16 physBlockIdx = ciso_block_map[blockIdx];
			if (physBlockIdx == 0xFFFF)
				return;

			const u32 blockOffset = (u32)(Offset64 % CISO_BLOCK_SIZE);
			const u32 physAddr = CISO_HEADER_SIZE + ((u32)physBlockIdx * CISO_BLOCK_SIZE) + blockOffset;
			if(wiiVCInternal)
				WDVD_FST_LSeek( physAddr );
			else
				f_lseek( &GameFile, physAddr );
		}
		else
		{
			if(wiiVCInternal)
				WDVD_FST_LSeek( Offset64 );
			else
				f_lseek( &GameFile, Offset64 );
		}
		LastOffset64 = Offset64;
	}
}

const u8 *ISORead(u32* Length, u32 Offset)
{
	if(CacheInited == 0)
	{
		if (*Length > DI_READ_BUFFER_LENGTH)
			*Length = DI_READ_BUFFER_LENGTH;
		ISOReadDirect(DI_READ_BUFFER, *Length, Offset);
		return DI_READ_BUFFER;
	}
	u32 i;

	for( i = 0; i < CACHE_MAX; ++i )
	{
		if(DC[i].Size == 0) continue;
		if( Offset >= DC[i].Offset && Offset + *Length <= DC[i].Offset + DC[i].Size )
		{
			//dbgprintf("DI: Cached Read Offset:%08X Size:%08X Buffer:%p\r\n", DC[i].Offset, DC[i].Size, DC[i].Data );
			return DC[i].Data + (Offset - DC[i].Offset);
		}
	}

	u64 Offset64 = Offset + ISOShift64;
	if( (Offset64 == LastOffset64) && (*Length < 0x8000) )
	{	//pre-load data, guessing
		u32 OriLength = *Length;
		while((*Length += OriLength) < 0x10000) ;
	}

	// case we ran out of positions
	if( TempCacheCount >= CACHE_MAX )
		TempCacheCount = 0;

	// case we filled up the cache
	if( (DataCacheOffset + *Length) >= DCacheLimit )
	{
		for( i = 0; i < CACHE_MAX; ++i )
			DC[i].Size = 0; //quickly delete old cache content
		DataCacheOffset = 0;
		TempCacheCount = 0;
	}

	u32 pos = TempCacheCount;
	TempCacheCount++;

	DC[pos].Data = DCCache + DataCacheOffset;
	DC[pos].Offset = Offset;
	DC[pos].Size = *Length;

	ISOReadDirect(DC[pos].Data, *Length, Offset64);

	DataCacheOffset += *Length;
	return DC[pos].Data;
}
