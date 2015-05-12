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
#include "ES.h"
#include "string.h"

#ifndef DEBUG_ES
#define dbgprintf(...)
#else
extern int dbgprintf( const char *fmt, ...);
#endif

char *path		= (char*)NULL;
u32 *size		= (u32*)NULL;
u64 *iTitleID	= (u64*)NULL;

static u64 TitleID ALIGNED(32);
static u32 KernelVersion ALIGNED(32);

static u8  *DITicket;

static u8  *CNTMap;
static u32 *CNTSize;
static u32 *CNTMapDirty;

static u64 *TTitles;

u64 *TTitlesO;
u32 TOCount;
u32 TOCountDirty;

u32 *KeyID = (u32*)NULL;

TitleMetaData *iTMD = (TitleMetaData *)NULL;			//used for information during title import
static u8 *iTIK		= (u8 *)NULL;						//used for information during title import

u16 TitleVersion;

// General ES functions

u32 ES_Init( u8 *MessageHeap )
{
//Used in Ioctlvs
	path		= (char*)malloca(		0x40,  32 );
	size		= (u32*) malloca( sizeof(u32), 32 );
	iTitleID	= (u64*) malloca( sizeof(u64), 32 );

	CNTMap		= (u8*)NULL;
	DITicket	= (u8*)NULL;
	KeyID		= (u32*)NULL;

	CNTSize		= (u32*)malloca( 4, 32 );
	CNTMapDirty	= (u32*)malloca( 4, 32 );
	*CNTMapDirty= 1;
	
	TTitles			= (u64*)NULL;
	TTitlesO		= (u64*)NULL;

	TOCount			= 0;
	TOCountDirty	= 0;
	TOCountDirty	= 1;

	u32 MessageQueue = mqueue_create( MessageHeap, 1 );

	device_register( "/dev/es", MessageQueue );

	u32 pid = GetPID();
	SetUID( pid, 0 );
	SetGID( pid, 0 );
#ifdef DEBUG_ES
	u32 version = KernelGetVersion();
	dbgprintf("ES:KernelVersion:%08X, %d\r\n", version, (version<<8)>>0x18 );
#endif
	ES_BootSystem();

	dbgprintf("ES:TitleID:%08x-%08x version:%d\r\n", (u32)((TitleID)>>32), (u32)(TitleID), TitleVersion );

	return MessageQueue;
}

s32 ES_BootSystem( void )
{
	char *path	= (char*)malloca( 0x40, 32 );
	u32 *size	= (u32*)malloca( sizeof(u32), 32 );

	u32 IOSVersion = 58;

	dbgprintf("ES:Loading IOS%d ...\r\n", IOSVersion );

//Load TMD of the requested IOS and build KernelVersion
	_sprintf( path, "/title/00000001/%08x/content/title.tmd", IOSVersion );

	TitleMetaData *TMD = (TitleMetaData *)NANDLoadFile( path, size );
	if( TMD == NULL )
	{
		dbgprintf("ES:Failed to open:\"%s\":%d\r\n", path, *size );
		free( path );
		free( size );
		Shutdown();
	}

	KernelVersion = TMD->TitleVersion;
	KernelVersion|= IOSVersion<<16;
	KernelSetVersion( KernelVersion );
#ifdef DEBUG_ES
	u32 version = KernelGetVersion();
	dbgprintf("ES:KernelVersion:%08X, %d\r\n", version, (version<<8)>>0x18 );
#endif
	free( TMD );

	s32 r = LoadModules( IOSVersion );
	dbgprintf("ES:ES_LoadModules(%d):%d\r\n", IOSVersion, r );
	if( r < 0 )
	{
		Shutdown();
	}
	mdelay( 300 ); //give modules time

	free( path );
	free( size );

	return r;
}

s32 LoadModules( u32 IOSVersion )
{
	//used later for decrypted
	KeyID = (u32*)malloca( sizeof(u32), 0x40 );
	char *path = malloca( 0x70, 0x40 );

	s32 r=0;
	int i;
	
	//load TMD	
	_sprintf( path, "/title/00000001/%08x/content/title.tmd", IOSVersion );

	u32 *size = (u32*)malloca( sizeof(u32), 0x40 );
	TitleMetaData *TMD = (TitleMetaData*)NANDLoadFile( path, size );
	if( TMD == NULL )
	{
		free( path );
		return *size;
	}

	if( TMD->ContentCount == 3 )	// STUB detected!
	{
		dbgprintf("ES:STUB IOS detected, falling back to IOS35\r\n");
		free( path );
		free( KeyID );
		free( size );
		return LoadModules( 35 );
	}
	
	dbgprintf("ES:ContentCount:%d\r\n", TMD->ContentCount );

	for( i=0; i < TMD->ContentCount; ++i )
	{
		//Don't load boot module
		if( TMD->BootIndex == TMD->Contents[i].Index )
			continue;

		if( TMD->Contents[i].Index == 0 )
			continue;
		//check if shared!
		if( TMD->Contents[i].Type & CONTENT_SHARED )
		{
			u32 ID = GetSharedContentID( TMD->Contents[i].SHA1 );

			if( (s32)ID == ES_FATAL )
			{
				dbgprintf("ES:Fatal error: required shared content not found!\r\n");
				dbgprintf("Hash:\r\n");
				hexdump( TMD->Contents[i].SHA1, 0x14 );
				Shutdown();

			} else {
				_sprintf( path, "/shared1/%08x.app", ID );
			}

		} else {
			_sprintf( path, "/title/00000001/%08x/content/%08x.app", IOSVersion, TMD->Contents[i].ID );
		}

		dbgprintf("ES:Loaded Module(%d):\"%s\"\r\n", i, path );
		r = LoadModule( path );
		if( r < 0 )
		{
			dbgprintf("ES:Fatal error: module failed to start!\r\n");
			dbgprintf("ret:%d\r\n", r );
			Shutdown();
		}
	}

	free( size );
	free( TMD );
	free( path );

	thread_set_priority( 0, 0x7F );
	mdelay(1000); //devices finish init
	thread_set_priority( 0, 0x50 );
	return ES_SUCCESS;
}
u64 GetTitleID( void )
{
	return TitleID;
}
void iCleanUpTikTMD( void )
{
	if( iTMD != NULL )
	{
		free( iTMD );
		iTMD = NULL;
	}
	if( iTIK != NULL )
	{
		free( iTIK );
		iTIK = (u8*)NULL;
	}
}
void iGetTMDView( TitleMetaData *TMD, u8 *oTMDView )
{
	u32 TMDViewSize = (TMD->ContentCount<<4) + 0x5C;
	u8 *TMDView		= (u8*)malloca( TMDViewSize, 32 );

	memset32( TMDView, 0, TMDViewSize );

	TMDView[0] = TMD->Version;

	*(u64*)(TMDView+0x04) =	TMD->SystemVersion;
	*(u64*)(TMDView+0x0C) =	TMD->TitleID;
	*(u32*)(TMDView+0x14) = TMD->TitleType;
	*(u16*)(TMDView+0x18) = TMD->GroupID;

	memcpy( TMDView+0x1A, (u8*)TMD + 0x19A, 0x3E );		// Region info

	*(u16*)(TMDView+0x58) = TMD->TitleVersion;
	*(u16*)(TMDView+0x5A) = TMD->ContentCount;

	if( TMD->ContentCount )					// Contents
	{
		int i;
		for( i=0; i < TMD->ContentCount; ++i )
		{
			*(u32*)(TMDView + i * 16 + 0x5C) = TMD->Contents[i].ID;
			*(u16*)(TMDView + i * 16 + 0x60) = TMD->Contents[i].Index;
			*(u16*)(TMDView + i * 16 + 0x62) = TMD->Contents[i].Type;
			*(u64*)(TMDView + i * 16 + 0x64) = TMD->Contents[i].Size;
		}
	}

////region free hack
//	memset8( TMDView+0x1E, 0, 16 );
//	*(u8*) (TMDView+0x21) = 0x0F;
//	*(u16*)(TMDView+0x1C) = 3;
////-

	memcpy( oTMDView, TMDView, TMDViewSize );
	free( TMDView );
}
s32 GetTMDView( u64 *TitleID, u8 *oTMDView )
{
	char *path	= (char*)malloca( 0x40, 32 );
	u32 *size	= (u32*) malloca( 4, 32 );

	_sprintf( path, "/title/%08x/%08x/content/title.tmd", (u32)(*TitleID>>32), (u32)*TitleID );

	u8 *data = (u8*)NANDLoadFile( path, size );
	if( data == NULL )
	{
		free( path );
		free( size );
		return ES_FATAL;
	}

	iGetTMDView( (TitleMetaData *)data, oTMDView );

	free( path );
	free( size );

	return ES_SUCCESS;
}
s32 GetUID( u64 *TitleID, u16 *UID )
{
	char *path	= (char*)malloca( 0x40, 32 );
	u32  *size	= (u32*) malloca( sizeof(u32), 32 );
	//char path[0x40] ALIGNED(32);
	//u32 size[sizeof(u32)] ALIGNED(32);

	_sprintf( path, "/sys/uid.sys");

	UIDSYS *uid = (UIDSYS *)NANDLoadFile( path, size );
	if( uid == NULL )
	{
		free( path );
		dbgprintf("ES:ESP_GetUID():Could not open \"/sys/uid.sys\"! Error:%d\r\n", *size );
		return *size;
	}

	*UID = 0xdead;

	u32 i;
	for( i=0; i * 12 < *size; ++i )
	{
		if( uid[i].TitleID == *TitleID )
		{
			*UID = uid[i].GroupID;
			break;
		}
	}

	free( uid );

	if( *UID == 0xdead )	// Title has no UID yet, add it
	{
		_sprintf( path, "/sys/uid.sys");

		s32 fd = IOS_Open( path, 2 );
		if( fd < 0 )
		{
			free( path );
			free( size );
			return fd;
		}

		// 1-2 UID: 0x1000

		*(vu64*)(path)	= *TitleID;
		*(vu32*)(path+8)= 0x00001000+*size/12+1;

		*UID = 0x1000+*size/12+1;
		
		dbgprintf("ES:TitleID not found adding new UID:0x%04x\r\n", *UID );

		s32 r = IOS_Seek( fd, 0, SEEK_END );
		if( r < 0 )
		{
			free( path );
			free( size );
			IOS_Close( fd );
			return r;
		}


		r = IOS_Write( fd, path, 12 );
		if( r != 12 || r < 0 )
		{
			free( path );
			free( size );
			IOS_Close( fd );
			return r;
		}

		IOS_Close( fd );
	}

	free( path );
	free( size );
	return 1;
}

s32 ES_CheckSharedContent( void *ContentHash )
{
	if( *CNTMapDirty )
	{
		dbgprintf("ES:Loading content.map...\r\n");

		if( CNTMap != NULL )
		{
			free( CNTMap );
			CNTMap = NULL;
		}

		CNTMap = (u8*)NANDLoadFile( "/shared1/content.map", CNTSize );

		if( CNTMap == NULL )
			return ES_FATAL;

		*CNTMapDirty = 0;
	}

	u32 ID=0;
	for( ID=0; ID < *CNTSize/0x1C; ++ID )
	{
		if( memcmp( CNTMap+ID*0x1C+8, ContentHash, 0x14 ) == 0 )
			return 1;
	}
	return 0;
}
/*
	Returns the content id for the supplied hash if found

	returns:
		0 < on error
		id on found
*/
s32 GetSharedContentID( void *ContentHash )
{
	if( *CNTMapDirty )
	{
		dbgprintf("ES:Loading content.map...\r\n");

		if( CNTMap != NULL )
		{
			free( CNTMap );
			CNTMap = (u8*)NULL;
		}

		CNTMap = (u8*)NANDLoadFile( "/shared1/content.map", CNTSize );

		if( CNTMap == NULL )
			return ES_FATAL;

		*CNTMapDirty = 0;
	}

	u32 ID=0;
	for( ID=0; ID < *CNTSize/0x1C; ++ID )
	{
		if( memcmp( CNTMap+ID*0x1C+8, ContentHash, 0x14 ) == 0 )
			return ID;
	}

	return ES_FATAL;
}

void GetTicketView( u8 *Ticket, u8 *oTicketView )
{
	u8 *TikView = (u8*)malloca( 0xD8, 32 );

	memset32( TikView, 0, 0xD8 );

	TikView[0] = Ticket[0x1BC];

	*(u64*)(TikView+0x04) = *(u64*)(Ticket+0x1D0);
	*(u32*)(TikView+0x0C) = *(u32*)(Ticket+0x1D8);
	*(u64*)(TikView+0x10) = *(u64*)(Ticket+0x1DC);
	*(u16*)(TikView+0x18) = *(u16*)(Ticket+0x1E4);
	*(u16*)(TikView+0x1A) = *(u16*)(Ticket+0x1E6);
	*(u32*)(TikView+0x1C) = *(u32*)(Ticket+0x1E8);
	*(u32*)(TikView+0x20) = *(u32*)(Ticket+0x1EC);
	*(u8*) (TikView+0x24) = *(u8*) (Ticket+0x1F0);

	memcpy( TikView+0x25, Ticket+0x1F1, 0x30 );

	*(u8*) (TikView+0x55) = *(u8*) (Ticket+0x221);

	memcpy( TikView+0x56, Ticket+0x222, 0x40 );

	u32 i;
	for( i=0; i < 7; ++i )
	{
		*(u32*)(i*8 + TikView + 0x98) = *(u32*)(i*8 + Ticket + 0x264);
		*(u32*)(i*8 + TikView + 0x9C) = *(u32*)(i*8 + Ticket + 0x268);
	}

	memcpy( oTicketView, TikView, 0xD8 );

	free( TikView );

	return;
}
