#include "debug.h"
#include "Config.h"
#include "common.h"
#include "string.h"
#include "alloc.h"
#include "wdvd.h"

#define SECTOR_SIZE 0x800

typedef struct {
	u32 dol_offset;
	u32 fst_offset;
	u32 fst_size;
	u32 fst_size2;
} __attribute__ ((packed)) FST_INFO;

typedef struct {
	u8 filetype;
	char name_offset[3];
	u32 fileoffset;
	u32 filelen;
} __attribute__((packed)) FST_ENTRY;

typedef struct
{
	bool inUse;
	u32 offset;
	s32 entry;
} OpenFstItem;

//these are the only memory consumed by this devotab
OpenFstItem openDir;
OpenFstItem openFile;
static u8 *fst_buffer = NULL;

//pointer to the name list at the end of fst_buffer
static char *name_table = NULL;
static FST_ENTRY *fst = NULL;

const char *FstName( const FST_ENTRY *entry )
{
	if( entry == &fst[ 0 ] )
	return NULL;

	return (char*)name_table + ( *((u32 *)entry) & 0x00ffffff );
}

static int strcmp_slash( const char *s1, const char *s2 )
{
	while( *s1 == *s2++ )
	if( *s1++ == 0 || *s1 == '/' )
		return( 0 );

	return( *(const unsigned char*)s1 - *(const unsigned char *)( s2 - 1 ) );
}

static int strlen_slash( const char *s )
{
	int ret = 0;
	while( *s && *s++ != '/' )
	ret++;

	return ret;
}

static u32 NextEntryInFolder( u32 current, u32 directory )
{
	u32 next = ( fst[ current ].filetype ? fst[ current ].filelen : current + 1 );
	if( next < fst[ directory ].filelen )
	return next;

	return 0;
}

static s32 EntryFromPath( const char *path, int d )
{
	if( !fst[ d ].filetype )
	{
		//gprintf("ERROR!!  %s is not a directory\n", FstName( &fst[ d ] ) );
		return -1;
	}

	u32 next = d + 1;

	FST_ENTRY *entry = &fst[ next ];

	while( next )
	{
		//gprintf( "\tcomparing: [ %u ] %s\n", next, FstName( entry ) );
		//does this entry match.
		//strlen_slash is used because if looking for "dvd:/gameboy/" it would return a false positive if it hit "dvd:/gameboy advance/" first
		if( !strcmp_slash( path, FstName( entry ) ) && ( strlen( FstName( entry ) ) == strlen_slash( path ) ) )
		{
			char *slash = strchr( path, '/' );
			if( slash && strlen( slash + 1 ) )
			{
			//we are looking for a file somewhere in this folder
			return EntryFromPath( slash + 1, next );
			}
			//this is the actual entry we were looking for
			return next;
		}

		//find the next entry in this folder
		next = NextEntryInFolder( next, d );
		entry = &fst[ next ];
	}

	//no entry with the given path was found
	return -1;
}

int WDVD_FST_Open( const char *path )
{
	if( openFile.inUse )
	{
		BootStatusError(-2, -10);
		mdelay(5000);
		//gprintf("tried to open more than 1 file at a time\n");
		return -1;
	}
	openFile.entry = EntryFromPath( path, 0 );
	if( openFile.entry < 0 )
	{
		BootStatusError(-2, -11);
		mdelay(5000);
		//gprintf("invalid filename: %s\n", path );
		return -1;
	}
	if( fst[ openFile.entry ].filetype )
	{
		BootStatusError(-2, -12);
		mdelay(5000);
		return -1;
	}
	openFile.offset = 0;
	openFile.inUse = true;

	return 0;
}

int WDVD_FST_Close()
{
	//gprintf("close\n");
	if( !openFile.inUse )
	{
		//gprintf("tried to close a file that isnt open\n" );
		return -1;
	}
	memset( &openFile, 0, sizeof( OpenFstItem ) );
	return 0;
}

static s32 _WDVD_FST_Seek( s32 pos )
{
	//gprintf("_FST_seek_r()\n" );

	if( pos < 0 || pos > fst[ openFile.entry ].filelen )
	{
		//gprintf("seek: shit\n");
		return -1;
	}
	openFile.offset = pos;

	return pos;
}

int WDVD_FST_Read(u8 *ptr, s32 pos, s32 len)
{
	//gprintf("read( %d )\n", fd );
	if( !openFile.inUse )
	{
		//gprintf("read: !openFile.inUse\n");
		return -1;
	}
	if( _WDVD_FST_Seek(pos) < 0)
	{
		//gprintf("read: _FST_Seek < 0\n");
		return 0;
	}
	if( openFile.offset >= fst[ openFile.entry ].filelen )
	{
		//gprintf("read: 2\n");
		return 0;
	}
	if( len + openFile.offset > fst[ openFile.entry ].filelen )
	{
		//gprintf("read: 3\n");
		len = fst[ openFile.entry ].filelen - openFile.offset;
	}
	if( len <= 0 )
	{
		return 0;
	}

	if( WDVD_Read( ptr, len, (u64)( ( (u64)fst[ openFile.entry ].fileoffset << 2LL) + openFile.offset ) ) < 0 )
	{
		//gprintf("read: failed\n");
		return -1;
	}

	openFile.offset += len;

	return len;

}

static bool read_disc() {
	//gprintf("read_disc()\n");

	s32 ret;
	FST_INFO fst_info;

	//find FST inside partition
	ret = WDVD_Read( (u8*)&fst_info, sizeof( FST_INFO ), 0x420LL );
	if( ret < 0 )
	{
		BootStatusError(-2, -8);
		mdelay(5000);
		//gprintf("WDVD_Read( fst_info ): %d\n", ret );
		goto error;
	}
	fst_info.fst_offset <<= 2;
	fst_info.fst_size <<= 2;
	//gprintf("fst offset: %08x\tsize: %08x\n", fst_info.fst_offset, fst_info.fst_size );
	//hexdump( &fst_info, 0x20 );

	fst_buffer = (u8*)malloca( fst_info.fst_size, 32 );
	if( !fst_buffer ) goto error;

	//read fst into memory
	ret = WDVD_Read( fst_buffer, fst_info.fst_size, fst_info.fst_offset );
	if( ret < 0 )
	{
		BootStatusError(-2, -9);
		mdelay(5000);
		//gprintf("WDVD_Read( fst_buffer ): %d\n", ret );
		goto error;
	}

	//set the pointers
	fst = (FST_ENTRY *)fst_buffer;
	u32 name_table_offset = fst->filelen * 0xC;
	name_table = (char *)( fst_buffer + name_table_offset );

	//clear the openFile and openDir entries
	memset( &openDir, 0, sizeof( OpenFstItem ) );
	memset( &openFile, 0, sizeof( OpenFstItem ) );

	//gprintf("read_disc ok\n");
	return true;

error:
	//gprintf("read_disc fail\n");
	if( fst_buffer )
	free( fst_buffer );

	fst = NULL;
	name_table = NULL;

	return false;
}

static bool alreadyMounted = false;

bool WDVD_FST_IsMounted()
{
	return alreadyMounted;
}

bool WDVD_FST_Mount()
{
	if( alreadyMounted )
		WDVD_FST_Unmount();

	bool success = read_disc();
	if( !success )
		WDVD_FST_Unmount();

	alreadyMounted = true;

	return success;
}

bool WDVD_FST_Unmount()
{
	//gprintf("FST_Unmount()\n");

	alreadyMounted = false;
	if( fst_buffer )
	{
		free( fst_buffer );
		fst_buffer = NULL;
	}
	name_table = NULL;
	fst = NULL;

	return true;
}

#define IOCTL_DI_READID		0x70
#define IOCTL_DI_READ		0x71
#define IOCTL_DI_WAITCVRCLOSE	0x79
#define IOCTL_DI_GETCOVER	0x88
#define IOCTL_DI_RESET		0x8A
#define IOCTL_DI_OPENPART	0x8B
#define IOCTL_DI_CLOSEPART	0x8C
#define IOCTL_DI_UNENCREAD	0x8D
#define PTABLE_OFFSET	0x40000

static u32 inbuf[ 8 ]  ALIGNED( 32 );
static u32 outbuf[ 8 ] ALIGNED( 32 );
static u32 tmpbuf[ 8 ] ALIGNED( 32 );
static u32 partBuf[0x1400] ALIGNED( 32 );

static const char di_fs[] ALIGNED( 32 ) = "/dev/di";
static s32 di_fd = -1;

bool partitionOpen = false;

s32 WDVD_Init(void) {
	if (di_fd < 0) {
	di_fd = IOS_Open(di_fs, 0);
	if (di_fd < 0)
		return di_fd;
	}
	//gprintf("WDVD_Init() ok di_fd: %d\n", di_fd );

	return 0;
}

s32 WDVD_Close(void) {
	if (di_fd >= 0) {
	IOS_Close(di_fd);
	di_fd = -1;
	}

	return 0;
}

s32 WDVD_GetHandle()
{
	return di_fd;
}

s32 WDVD_Reset(void) {
	s32 ret;

	memset(inbuf, 0, sizeof(inbuf));

	inbuf[0] = IOCTL_DI_RESET << 24;
	inbuf[1] = 1;

	ret = IOS_Ioctl(di_fd, IOCTL_DI_RESET, inbuf, sizeof(inbuf), outbuf, sizeof(outbuf));
	if (ret < 0)
	return ret;

	return (ret == 1) ? 0 : -ret;
}

s32 WDVD_ReadDiskId(void *id) {
	s32 ret;

	memset(inbuf, 0, sizeof(inbuf));

	inbuf[0] = IOCTL_DI_READID << 24;

	ret = IOS_Ioctl(di_fd, IOCTL_DI_READID, inbuf, sizeof(inbuf), outbuf, sizeof(outbuf));
	if (ret < 0)
	return ret;

	if (ret == 1) {
		memcpy(id, outbuf, 32);
		return 0;
	}

	return -ret;
}

s32 WDVD_OpenPartition(u64 offset) {
	u8 *vector = NULL;

	s32 ret;

	vector = (u8 *)partBuf;

	memset(partBuf, 0, 0x5000);

	partBuf[0] = (u32)(partBuf + 0x10);
	partBuf[1] = 0x20;
	partBuf[3] = 0x024A;
	partBuf[6] = (u32)(partBuf + 0x380);
	partBuf[7] = 0x49E4;
	partBuf[8] = (u32)(partBuf + 0x360);
	partBuf[9] = 0x20;

	partBuf[(0x40 >> 2)]     = IOCTL_DI_OPENPART << 24;
	partBuf[(0x40 >> 2) + 1] = offset >> 2;

	sync_after_write(partBuf, 0x5000);

	ret = IOS_Ioctlv(di_fd, IOCTL_DI_OPENPART, 3, 2, (ioctlv *)vector);

	if (ret < 0)
		return ret;

	return (ret == 1) ? 0 : -ret;
}

s32 WDVD_ClosePartition(void) {
	s32 ret;

	partitionOpen = false;

	memset(inbuf, 0, sizeof(inbuf));

	inbuf[0] = IOCTL_DI_CLOSEPART << 24;

	ret = IOS_Ioctl(di_fd, IOCTL_DI_CLOSEPART, inbuf, sizeof(inbuf), NULL, 0);
	if (ret < 0)
	{
		BootStatusError(-2, -18);
		mdelay(5000);
		return ret;
	}
	return (ret == 1) ? 0 : -ret;
}

s32 WDVD_UnencryptedRead(void *buf, u32 len, u64 offset) {
	s32 ret;

	memset(inbuf, 0, sizeof(inbuf));

	inbuf[0] = IOCTL_DI_UNENCREAD << 24;
	inbuf[1] = len;
	inbuf[2] = (u32)(offset >> 2);

	ret = IOS_Ioctl(di_fd, IOCTL_DI_UNENCREAD, inbuf, sizeof(inbuf), buf, len);
	if (ret < 0)
	{
		BootStatusError(-2, -17);
		mdelay(5000);
		return ret;
	}
	return (ret == 1) ? 0 : -ret;
}

s32 WDVD_Read(void *buf, u32 len, u64 offset) {
	s32 ret;

	memset(inbuf, 0, sizeof(inbuf));

	inbuf[0] = IOCTL_DI_READ << 24;
	inbuf[1] = len;
	inbuf[2] = (u32)(offset >> 2);

	ret = IOS_Ioctl(di_fd, IOCTL_DI_READ, inbuf, sizeof(inbuf), buf, len);
	if (ret < 0)
	return ret;

	return (ret == 1) ? 0 : -ret;
}

s32 WDVD_WaitForDisc(void) {
	s32 ret;

	memset(inbuf, 0, sizeof(inbuf));

	inbuf[0] = IOCTL_DI_WAITCVRCLOSE << 24;

	ret = IOS_Ioctl(di_fd, IOCTL_DI_WAITCVRCLOSE, inbuf, sizeof(inbuf), outbuf, sizeof(outbuf));
	if (ret < 0)
	return ret;

	return (ret == 1) ? 0 : -ret;
}

s32 WDVD_GetCoverStatus(u32 *status) {
	s32 ret;

	memset(inbuf, 0, sizeof(inbuf));

	inbuf[0] = IOCTL_DI_GETCOVER << 24;

	ret = IOS_Ioctl(di_fd, IOCTL_DI_GETCOVER, inbuf, sizeof(inbuf), outbuf, sizeof(outbuf));
	if (ret < 0)
	return ret;

	if (ret == 1) {
	memcpy(status, outbuf, sizeof(u32));

	return 0;
	}

	return -ret;
}

s32 WDVD_FindPartition(u64 *outbuf)
{
	u64 offset = 0, table_offset = 0;

	u32 cnt, nb_partitions;
	s32 ret;

	sync_before_read(tmpbuf, 0x20);
	ret = WDVD_UnencryptedRead(tmpbuf, 0x20, PTABLE_OFFSET);
	if (ret < 0)
	{
		BootStatusError(-2, -14);
		mdelay(5000);
		return ret;
	}

	nb_partitions = tmpbuf[0];
	table_offset  = tmpbuf[1] << 2;

	sync_before_read(tmpbuf, 0x20);
	ret = WDVD_UnencryptedRead(tmpbuf, 0x20, table_offset);
	if (ret < 0)
	{
		BootStatusError(-2, -15);
		mdelay(5000);
		return ret;
	}

	for (cnt = 0; cnt < nb_partitions; cnt++) {
		u32 type = tmpbuf[cnt * 2 + 1];

		if (!type)
		{
			offset = tmpbuf[cnt * 2] << 2;
			break;
		}
	}

	if (!offset)
	{
		BootStatusError(-2, -16);
		mdelay(5000);
		return -1;
	}
	*outbuf = offset;

	return 0;
}

s32 WDVD_OpenDataPartition()
{
	u64 dataPartitionOffset;
	u32 status = 0;

	s32 ret = WDVD_GetCoverStatus( &status );
	if( ret < 0 )
	{
		BootStatusError(-2, -1);
		mdelay(5000);
		//gprintf("WDVD_GetCoverStatus(): %d\n", ret );
		return ret;
	}
	if( status != 2 )
	{
		BootStatusError(-2, -2);
		mdelay(5000);
		//gprintf("discstatus: %d\n", status );
		return -1;
	}

	ret = WDVD_ClosePartition();
	if( ret < 0 )
	{
		BootStatusError(-2, -3);
		mdelay(5000);
		//gprintf("WDVD_ClosePartition(): %d\n", ret );
		return ret;
	}

	ret = WDVD_Reset();
	if( ret < 0 )
	{
		BootStatusError(-2, -4);
		mdelay(5000);
		//gprintf("WDVD_Reset(): %d\n", ret );
		return ret;
	}

	ret = WDVD_ReadDiskId( tmpbuf );
	if( ret < 0 )
	{
		BootStatusError(-2, -5);
		mdelay(5000);
		//gprintf("WDVD_ReadDiskId(): %d\n", ret );
		return ret;
	}

	ret = WDVD_FindPartition( &dataPartitionOffset );
	if( ret < 0 )
	{
		BootStatusError(-2, -6);
		mdelay(5000);
		//gprintf("WDVD_FindPartition(): %d\n", ret );
		return ret;
	}

	ret = WDVD_OpenPartition( dataPartitionOffset );
	if( ret < 0 )
	{
		BootStatusError(-2, -7);
		mdelay(5000);
		//gprintf("WDVD_OpenPartition( %016llx ): %d\n", dataPartitionOffset, ret );
		return ret;
	}

	partitionOpen = true;

	//gprintf("opened partition @ %016llx\n", dataPartitionOffset );

	return 0;
}
