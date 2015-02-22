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
#include "DI.h"

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

extern u32 Region;

u32 FSTInit( char *GamePath )
{
	char Path[256];
	FIL fd;
	u32 read;

	FSTable = NULL;
	
	_sprintf( Path, "%ssys/boot.bin", GamePath );
	if( f_open_char( &fd, Path, FA_READ ) != FR_OK )
	{
		dbgprintf( "DIP:[%s] Failed to open!\r\n", Path );
		return 0;

	} else {
		u8 *rbuf = (u8*)malloc( 0x100 );

		f_lseek( &fd, 0 );
		f_read( &fd, rbuf, 0x100, &read );

		/* Set Low Mem */
		memcpy( (void*)0, rbuf, 0x20);
		sync_after_write( (void*)0, 0x20 );

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
void FSTCleanup()
{
	free(FC);
	FC = NULL;
	FSTMode = 0;
	FSTable = NULL;
}
u8* FSTRead(char *GamePath, u32* Length, u32 Offset)
{
	if (*Length > DI_READ_BUFFER_LENGTH)
		*Length = DI_READ_BUFFER_LENGTH;
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
					f_read( &(FC[i].File), DI_READ_BUFFER, ((*Length)+31)&(~31), &read );
					return DI_READ_BUFFER;
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

						f_open_char( &(FC[FCEntry].File), Path, FA_READ );

						FC[FCEntry].Size	= fe[i].FileLength;
						FC[FCEntry].Offset	= fe[i].FileOffset;
						FCState[FCEntry]	= 0x23;

						f_lseek( &(FC[FCEntry].File), nOffset );
						f_read( &(FC[FCEntry].File), DI_READ_BUFFER, *Length, &read );

						FCEntry++;
					}
				}
			}
		}

	} else if ( Offset >= FSTableOffset ) {
		
		Offset -= FSTableOffset;
		
		_sprintf( Path, "%ssys/fst.bin", GamePath );
		if( f_open_char( &fd, Path, FA_READ ) != FR_OK )
		{
			dbgprintf( "DIP:[%s] Failed to open!\r\n", Path );
			return DI_READ_BUFFER;
		} else {
			//dbgprintf( "DIP:[fst.bin] Offset:%08X Size:%08X\r\n", Offset, *Length );
			
			f_lseek( &fd, Offset );
			f_read( &fd, DI_READ_BUFFER, *Length, &read );
			f_close( &fd );

			return DI_READ_BUFFER;
		}

	} else if ( Offset >= dolOffset ) {
		
		Offset -= dolOffset;
		
		_sprintf( Path, "%ssys/main.dol", GamePath );
		if( f_open_char( &fd, Path, FA_READ ) != FR_OK )
		{
			dbgprintf( "DIP:[%s] Failed to open!\r\n", Path );
			return DI_READ_BUFFER;
		} else {
			//dbgprintf( "DIP:[main.dol] Offset:%08X Size:%08X\r\n", Offset, *Length );
			
			f_lseek( &fd, Offset );
			f_read( &fd, DI_READ_BUFFER, *Length, &read );
			f_close( &fd );

			return DI_READ_BUFFER;
		}

	} else if ( Offset >= 0x2440 ) {
		
		Offset -= 0x2440;
		
		_sprintf( Path, "%ssys/apploader.img", GamePath );
		if( f_open_char( &fd, Path, FA_READ ) != FR_OK )
		{
			dbgprintf( "DIP:[%s] Failed to open!\r\n", Path );
			return DI_READ_BUFFER;
		} else {
			//dbgprintf( "DIP:[apploader.img] Offset:%08X Size:%08X\r\n", Offset, *Length );
			
			f_lseek( &fd, Offset );
			f_read( &fd, DI_READ_BUFFER, *Length, &read );
			f_close( &fd );

			return DI_READ_BUFFER;
		}

	} else if ( Offset >= 0x440 ) {

		Offset -= 0x440;
		
		_sprintf( Path, "%ssys/bi2.bin", GamePath );
		if( f_open_char( &fd, Path, FA_READ ) != FR_OK )
		{
			dbgprintf( "DIP:[%s] Failed to open!\r\n", Path );
			return DI_READ_BUFFER;
		} else {
			//dbgprintf( "DIP:[bi2.bin] Offset:%08X Size:%08X\r\n", Offset, *Length );
			
			f_lseek( &fd, Offset );
			f_read( &fd, DI_READ_BUFFER, *Length, &read );

			f_close( &fd );

			Region = *(vu32*)(DI_READ_BUFFER+0x18);

			return DI_READ_BUFFER;
		}

	} else {
		_sprintf( Path, "%ssys/boot.bin", GamePath );
		if( f_open_char( &fd, Path, FA_READ ) != FR_OK )
		{
			dbgprintf( "DIP:[%s] Failed to open!\r\n", Path );
			return DI_READ_BUFFER;
		} else {
			//dbgprintf( "DIP:[boot.bin] Offset:%08X Size:%08X\r\n", Offset, *Length );
			
			f_lseek( &fd, Offset );
			f_read( &fd, DI_READ_BUFFER, *Length, &read );

			f_close( &fd );

			return DI_READ_BUFFER;
		}
	}
	return DI_READ_BUFFER;
}
