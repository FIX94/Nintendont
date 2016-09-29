#ifndef __FST_H__
#define __FST_H__

#include "global.h"
#include "ff.h"

#define FILECACHE_MAX	2

u32	FSTInit	( char *GamePath );
void	FSTCleanup ( void );
u8*	FSTRead	( char *GamePath, u32* Length, u32 Offset );

void	CacheInit( char *Table, bool ForceReinit );
void	CacheFile( char *FileName, char *Table );
u8*	CacheRead( u32* Length, u32 Offset );

#endif
