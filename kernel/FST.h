#ifndef __FST_H__
#define __FST_H__

#include "global.h"
#include "ff.h"

#define FILECACHE_MAX	2

u32 FSTInit( const char *GamePath );
void FSTCleanup( void );
const u8* FSTRead( const char *GamePath, u32* Length, u32 Offset );

void CacheInit( char *Table, bool ForceReinit );
void CacheFile( const char *FileName, char *Table );
const u8* CacheRead( u32* Length, u32 Offset );

#endif
