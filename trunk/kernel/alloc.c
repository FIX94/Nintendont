#include "alloc.h"
#include "vsprintf.h"
#include "debug.h"

void *malloc( u32 size )
{
	void *ptr = heap_alloc( 0, size );
	if( ptr == NULL )
	{
		//dbgprintf("Malloc:%p Size:%08X FAILED\r\n", ptr, size );
		Shutdown();
	}
	return ptr;
}
void *malloca( u32 size, u32 align )
{
	void *ptr = heap_alloc_aligned( 0, size, align );
	if( ptr == NULL )
	{
		//dbgprintf("Malloca:%p Size:%08X FAILED\r\n", ptr, size );
		Shutdown();
	}
	return ptr;
}
void free( void *ptr )
{
	if( ptr != NULL )
		heap_free( 0, ptr );

	//dbgprintf("Free:%p\r\n", ptr );

	return;
}