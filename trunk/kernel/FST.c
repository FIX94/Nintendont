/*

Nintendont (Kernel) - Playing Gamecubes in Wii mode on a Wii U

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
#include "FST.h"
#include "ff.h"
#include "common.h"
#include "alloc.h"
#include "vsprintf.h"
#include "Config.h"

#ifndef DEBUG_FST
#define dbgprintf(...)
#else
extern int dbgprintf( const char *fmt, ...);
#endif

static u8 *FSTable ALIGNED(32);
u32 ApploaderSize=0;
u32 dolOffset=0;
u32 FSTMode = 0;
u32 FSTableSize=0;
u32 FSTableOffset=0;

u32 FCEntry=0;
FileCache *FC;
u32 FCState[FILECACHE_MAX];

extern FIL GameFile;

u32 CacheIsInit		= 0;
u32 DataCacheCount	= 0;
u32 TempCacheCount	= 0;
u32 DataCacheOffset = 0;
u8 *DCCache		= (u8*)0x11280000;
DataCache DC[DATACACHE_MAX];

extern u32 Region;

u32 FSTInit( char *GamePath )
{
	char Path[256];
	FIL fd;
	u32 read;

	FSTable = NULL;
	
	_sprintf( Path, "%ssys/boot.bin", GamePath );
	if( f_open( &fd, Path, FA_READ ) != FR_OK )
	{
		dbgprintf( "DIP:[%s] Failed to open!\r\n", Path );
		return 0;

	} else {
		u8 *rbuf = (u8*)malloc( 0x100 );
		
		f_lseek( &fd, 0 );
		f_read( &fd, rbuf, 0x100, &read );

		dbgprintf("DIP:Loading game %.6s: %s\r\n", rbuf, (char *)(rbuf+0x20));

		//Read DOL/FST offset/sizes for later usage
		f_lseek( &fd, 0x0420 );
		f_read( &fd, rbuf, 0x20, &read );

		dolOffset		= *(u32*)(rbuf);
		FSTableOffset	= *(u32*)(rbuf+4);
		FSTableSize		= *(u32*)(rbuf+8);

		free( rbuf );
		
		dbgprintf( "DIP:FSTableOffset:%08X\r\n", FSTableOffset );
		dbgprintf( "DIP:FSTableSize:  %08X\r\n", FSTableSize );
		dbgprintf( "DIP:DolOffset:    %08X\r\n", dolOffset );	

		FSTMode = 1;

		FC = (FileCache*)malloc( sizeof(FileCache) * FILECACHE_MAX );

		f_close( &fd );
	}

	//Init cache
	u32 count = 0;
	for( count=0; count < FILECACHE_MAX; ++count )
	{
		FCState[count] = 0xdeadbeef;
	}

	return 1;
}
void FSTRead( char *GamePath, u8 *Buffer, u32 Length, u32 Offset )
{
	char Path[256];
	FIL fd;
	u32 read;
	int i,j;
	
	if( Offset >= FSTableOffset+FSTableSize ) {	
		//Get FSTTable offset from low memory, must be set by apploader
		if( FSTable == NULL )
		{
			FSTable	= (u8*)((*(vu32*)0x38) & 0x7FFFFFFF);
			//dbgprintf("DIP:FSTOffset:  %08X\r\n", (u32)FSTable );
		}

		//try cache first!
		for( i=0; i < FILECACHE_MAX; ++i )
		{
			if( FCState[i] == 0xdeadbeef )
				continue;

			if( Offset >= FC[i].Offset )
			{
				u64 nOffset = Offset - FC[i].Offset;
				if( nOffset < FC[i].Size )
				{
					//dbgprintf("DIP:[Cache:%02d][%08X:%05X]\r\n", i, (u32)(nOffset>>2), Length );
					f_lseek( &(FC[i].File), nOffset );
					f_read( &(FC[i].File), Buffer, ((Length)+31)&(~31), &read );
					return;
				}
			}
		}

		//The fun part!

		u32 Entries = *(u32*)(FSTable+0x08);
		char *NameOff = (char*)(FSTable + Entries * 0x0C);
		FEntry *fe = (FEntry*)(FSTable);

		u32 Entry[16];
		u32 LEntry[16];
		u32 level=0;

		for( i=1; i < Entries; ++i )
		{
			if( level )
			{
				while( LEntry[level-1] == i )
				{
					//printf("[%03X]leaving :\"%s\" Level:%d\r\n", i, buffer + NameOff + swap24( fe[Entry[level-1]].NameOffset ), level );
					level--;
				}
			}

			if( fe[i].Type )
			{
				//Skip empty folders
				if( fe[i].NextOffset == i+1 )
					continue;

				//printf("[%03X]Entering:\"%s\" Level:%d leave:%04X\r\n", i, buffer + NameOff + swap24( fe[i].NameOffset ), level, swap32( fe[i].NextOffset ) );
				Entry[level] = i;
				LEntry[level++] = fe[i].NextOffset;
				if( level > 15 )  // something is wrong!
					break;
			} else {
				if( Offset >= fe[i].FileOffset )
				{
					u32 nOffset = (Offset - fe[i].FileOffset);
					if( nOffset < fe[i].FileLength )
					{
						//dbgprintf("DIP:Offset:%08X FOffset:%08X Dif:%08X Flen:%08X nOffset:%08X\r\n", Offset, fe[i].FileOffset, Offset-fe[i].FileOffset, fe[i].FileLength, nOffset );

						//Do not remove!
						memset32( Path, 0, 256 );					
						_sprintf( Path, "%sroot/", GamePath );

						for( j=0; j<level; ++j )
						{
							if( j )
								Path[strlen(Path)] = '/';
							memcpy( Path+strlen(Path), NameOff + fe[Entry[j]].NameOffset, strlen(NameOff + fe[Entry[j]].NameOffset ) );
						}
						if( level )
							Path[strlen(Path)] = '/';
						memcpy( Path+strlen(Path), NameOff + fe[i].NameOffset, strlen(NameOff + fe[i].NameOffset) );
						
						if( FCEntry >= FILECACHE_MAX )
							FCEntry = 0;

						if( FCState[FCEntry] != 0xdeadbeef )
						{
							f_close( &(FC[FCEntry].File) );
							FCState[FCEntry] = 0xdeadbeef;
						}

						Asciify( Path );

						//dbgprintf("DIP:[%s]\r\n", Path+strlen(GamePath)+5 );

						f_open( &(FC[FCEntry].File), Path, FA_READ );

						FC[FCEntry].Size	= fe[i].FileLength;
						FC[FCEntry].Offset	= fe[i].FileOffset;
						FCState[FCEntry]	= 0x23;

						f_lseek( &(FC[FCEntry].File), nOffset );
						f_read( &(FC[FCEntry].File), Buffer, Length, &read );

						FCEntry++;
					}
				}
			}
		}

	} else if ( Offset >= FSTableOffset ) {
		
		Offset -= FSTableOffset;
		
		_sprintf( Path, "%ssys/fst.bin", GamePath );
		if( f_open( &fd, Path, FA_READ ) != FR_OK )
		{
			dbgprintf( "DIP:[%s] Failed to open!\r\n", Path );
			return;
		} else {
			//dbgprintf( "DIP:[fst.bin] Offset:%08X Size:%08X\r\n", Offset, Length );
			
			f_lseek( &fd, Offset );
			f_read( &fd, Buffer, Length, &read );
			f_close( &fd );

			return;
		}

	} else if ( Offset >= dolOffset ) {
		
		Offset -= dolOffset;
		
		_sprintf( Path, "%ssys/main.dol", GamePath );
		if( f_open( &fd, Path, FA_READ ) != FR_OK )
		{
			dbgprintf( "DIP:[%s] Failed to open!\r\n", Path );
			return;
		} else {
			//dbgprintf( "DIP:[main.dol] Offset:%08X Size:%08X\r\n", Offset, Length );
			
			f_lseek( &fd, Offset );
			f_read( &fd, Buffer, Length, &read );
			f_close( &fd );

			return;
		}

	} else if ( Offset >= 0x2440 ) {
		
		Offset -= 0x2440;
		
		_sprintf( Path, "%ssys/apploader.img", GamePath );
		if( f_open( &fd, Path, FA_READ ) != FR_OK )
		{
			dbgprintf( "DIP:[%s] Failed to open!\r\n", Path );
			return;
		} else {
			//dbgprintf( "DIP:[apploader.img] Offset:%08X Size:%08X\r\n", Offset, Length );
			
			f_lseek( &fd, Offset );
			f_read( &fd, Buffer, Length, &read );
			f_close( &fd );

			return;
		}

	} else if ( Offset >= 0x440 ) {

		Offset -= 0x440;
		
		_sprintf( Path, "%ssys/bi2.bin", GamePath );
		if( f_open( &fd, Path, FA_READ ) != FR_OK )
		{
			dbgprintf( "DIP:[%s] Failed to open!\r\n", Path );
			return;
		} else {
			//dbgprintf( "DIP:[bi2.bin] Offset:%08X Size:%08X\r\n", Offset, Length );
			
			f_lseek( &fd, Offset );
			f_read( &fd, Buffer, Length, &read );

			f_close( &fd );

			Region = *(vu32*)(Buffer+0x18);

			return;
		}

	} else {
		_sprintf( Path, "%ssys/boot.bin", GamePath );
		if( f_open( &fd, Path, FA_READ ) != FR_OK )
		{
			dbgprintf( "DIP:[%s] Failed to open!\r\n", Path );
			return;
		} else {
			//dbgprintf( "DIP:[boot.bin] Offset:%08X Size:%08X\r\n", Offset, Length );
			
			f_lseek( &fd, Offset );
			f_read( &fd, Buffer, Length, &read );

			f_close( &fd );

			return;
		}
	}
}
void CacheInit( char *Table )
{
	if(CacheIsInit)
		return;

	memset32(DC, 0, sizeof(DataCache) * DATACACHE_MAX);
	sync_after_write(DC, sizeof(DataCache) * DATACACHE_MAX);

	FIL file;
	u32 read;
	u32 offset = 0;
	char *path = (char*)malloc( 128 );

	_sprintf( path, "%s", ConfigGetGamePath() );

	offset = strlen(path);

	//Clear ISO name
	while(offset > 1)
	{
		if( path[offset] == '/' )
			break;

		path[offset] = 0;
		offset--; 
	}

	memcpy( path + strlen(path), "cache.txt", 11 );
	
	dbgprintf("DIP:opening:\"%s\"\r\n", path );

	if( f_open( &file, path, FA_OPEN_EXISTING|FA_READ ) == FR_OK )
	{
		char *data = (char*)malloc( file.fsize );
		if( data == NULL )
		{
			dbgprintf("DIP:Failed to alloc %u bytes\r\n", file.fsize );
		} else {

			f_read( &file, data, file.fsize, &read );
			f_close( &file );

			offset		= 0;
			u32 len		= strlen( data );
			u32 elen	= 0;
			char *str	= data;

			while(offset <= len)
			{
				elen = 0;
				//find \0xa or \0xd
				while(1)
				{
					if( str[offset+elen] == 0x0A || str[offset+elen] == 0x0D )
						break;
					if( offset+elen >= len )
						break;

					elen++;
				}

				if( offset+elen >= len )
					break;

				memset( path, 0, 128 );
				memcpy( path, str + offset, elen );
				CacheFile( path, Table );

				offset += elen;

				while(offset <= len)
				{
					if( str[offset] == 0x0A || str[offset] == 0x0D )
					{
						offset++;
						continue;
					}
					break;
				}
			}

			free(data);
		}
	} else {
		dbgprintf("DIP:Couldn't open:\"%s\"\r\n", path );
	}

	free(path);

	CacheIsInit++;
}

void CacheFile( char *FileName, char *Table )
{
	if( DataCacheCount >= DATACACHE_MAX )
		return;
	if( DataCacheOffset >= 0x1D80000 )
		return;

	u32 Entries		= *(u32*)(Table + 0x08);
	char *NameOff	= (char*)(Table + Entries * 0x0C);
	FEntry *fe		= (FEntry*)(Table);

	//u32 Entry[16];
	u32 LEntry[16];
	u32 level=0;
	u32 i=0;
	u32 read;

	for( i=1; i < Entries; ++i )
	{
		if( level )
		{
			while( LEntry[level-1] == i )
			{
				//printf("[%03X]leaving :\"%s\" Level:%d\n", i, buffer + NameOff + swap24( fe[Entry[level-1]].NameOffset ), level );
				level--;
			}
		}

		if( fe[i].Type )
		{
			//Skip empty folders
			if( fe[i].NextOffset == i+1 )
				continue;

			//printf("[%03X]Entering:\"%s\" Level:%d leave:%04X\n", i, buffer + NameOff + swap24( fe[i].NameOffset ), level, swap32( fe[i].NextOffset ) );
			//Entry[level] = i;
			LEntry[level++] = fe[i].NextOffset;
			if( level > 15 )  // something is wrong!
				break;
		} else {
			if( strcmp( FileName, NameOff + fe[i].NameOffset ) == 0 )
			{
				dbgprintf("[%s] Offset:%08X Size:%u\r\n", NameOff + fe[i].NameOffset, fe[i].FileOffset, fe[i].FileLength );

				if( (DataCacheOffset <= 0xD80000) && ((DataCacheOffset + fe[i].FileLength) >= 0xD80000) )
					DataCacheOffset = 0xDA0000;

				f_lseek( &GameFile, fe[i].FileOffset );
				f_read( &GameFile, DCCache + DataCacheOffset, fe[i].FileLength, &read );
				sync_after_write( DCCache + DataCacheOffset, fe[i].FileLength );

				DC[DataCacheCount].Data		= DCCache + DataCacheOffset;
				DC[DataCacheCount].Size		= (fe[i].FileLength + 3) & (~3);
				DC[DataCacheCount].Offset	= fe[i].FileOffset;

				DataCacheOffset += (fe[i].FileLength + 31) & (~31);

				dbgprintf("DI: Cached Offset:%08X Size:%08X Buffer:%p\r\n", fe[i].FileOffset, fe[i].FileLength, DC[DataCacheCount].Data );

				DataCacheCount++;
				return;
			}
		}
	}
}
u32 LastLength = 0, StartOffset = 0x1D80000, StartPos = 0;
u8 *CacheRead( u8 *Buffer, u32 Length, u32 Offset )
{
	u32 read, i;

	// nintendont loader asking, just return
	if(CacheIsInit == 0)
	{
		f_lseek( &GameFile, Offset );
		f_read( &GameFile, Buffer, Length, &read );
		return Buffer;
	}

	// try cache.txt first
	if(DataCacheCount > 0)
	{
		for( i=0; i < DataCacheCount; ++i )
		{
			if( Offset >= DC[i].Offset )
			{
				u64 nOffset = Offset - DC[i].Offset;
				if( nOffset < DC[i].Size )
				{
					//dbgprintf("DI: Cached Read Offset:%08X Size:%08X Buffer:%p\r\n", DC[i].Offset, DC[i].Size, DC[i].Data );
					return DC[i].Data + nOffset;
				}
			}
		}
		f_lseek( &GameFile, Offset );
		f_read( &GameFile, Buffer, Length, &read );
		return Buffer;
	}


	// else
	for( i = 0; i < DATACACHE_MAX; ++i )
	{
		if( Offset == DC[i].Offset && Length == DC[i].Size )
		{
			//dbgprintf("DI: Cached Read Offset:%08X Size:%08X Buffer:%p\r\n", DC[i].Offset, DC[i].Size, DC[i].Data );
			return DC[i].Data;
		}
	}

	// dont waste cache
	if( Length == LastLength )
	{
		f_lseek( &GameFile, Offset );
		f_read( &GameFile, Buffer, Length, &read );
		return Buffer;
	}
	// new length, just cache it
	LastLength = Length;

	// case we ran out of positions
	if( TempCacheCount >= DATACACHE_MAX )
	{
		TempCacheCount = 0;
		sync_after_write(&TempCacheCount, 4);
	}

	// case we just filled up the cache
	if( (DataCacheOffset + Length) >= 0x1D80000 )
	{
		for( i = TempCacheCount; i < TempCacheCount + DATACACHE_MAX; ++i )
		{
			u32 c = i % DATACACHE_MAX;
			if(DC[c].Size == 0)
				continue;
			// found the point to restart cycle
			DataCacheOffset = 0;
			// set next point in reference
			StartPos = (c + 1) % DATACACHE_MAX;
			StartOffset = DC[StartPos].Data - DCCache;
			break;
		}
	}

	// case we overwrite the start
	if( (DataCacheOffset + Length) >= StartOffset )
	{
		for( i = StartPos; i < StartPos + DATACACHE_MAX; ++i )
		{
			u32 c = i % DATACACHE_MAX;
			DC[c].Offset = 0; // basically makes sure we wont overwrite it
			if( (DataCacheOffset + Length) < (DC[c].Data - DCCache) ) // new pos wouldnt overwrite it
			{
				StartPos = c;
				StartOffset = DC[StartPos].Data - DCCache;
				break;
			}
			DC[c].Data = 0;
		}
	}

	u32 pos = TempCacheCount;
	TempCacheCount++;

	DC[pos].Data = DCCache + DataCacheOffset;
	DC[pos].Offset = Offset;
	DC[pos].Size = Length;
	sync_after_write(&DC[pos], sizeof(DataCache));

	f_lseek( &GameFile, Offset );
	f_read( &GameFile, DC[pos].Data, Length, &read );

	DataCacheOffset += Length;
	return DC[pos].Data;
}
