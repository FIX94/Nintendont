#ifndef __ALLOC_H__
#define __ALLOC_H__

#include "syscalls.h"

void *malloc( u32 size );
void *malloca( u32 size, u32 align );
void free( void *ptr );
#endif