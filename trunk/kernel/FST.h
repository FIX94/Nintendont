#ifndef __FST_H__
#define __FST_H__

#include "global.h"
#include "ff.h"

typedef struct
{
	union
	{
		struct
		{
			u32 Type		:8;
			u32 NameOffset	:24;
		};
		u32 TypeName;
	};
	union
	{
		struct		// File Entry
		{
			u32 FileOffset;
			u32 FileLength;
		};
		struct		// Dir Entry
		{
			u32 ParentOffset;
			u32 NextOffset;
		};
		u32 entry[2];
	};
} FEntry;

typedef struct
{
	u32 Offset;
	u32 Size;
	FIL File;
} FileCache;

typedef struct
{
	u32 Offset;
	u32 Size;
	u8 *Data;
} DataCache;

#define FILECACHE_MAX	2
#define DATACACHE_MAX	220

u32		FSTInit	( char *GamePath );
void	FSTCleanup ( void );
u8*		FSTRead	( char *GamePath, u32* Length, u32 Offset );

void	CacheInit( char *Table, bool ForceReinit );
void	CacheFile( char *FileName, char *Table );
u8*		CacheRead( u32* Length, u32 Offset );

#endif
