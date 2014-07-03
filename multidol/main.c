/*
	TinyLoad - a simple region free (original) game launcher in 4k

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

/* This code comes from HBC's stub which was based on dhewg's geckoloader stub */
// Copyright 2008-2009  Andre Heider  <dhewg@wiibrew.org>
// Copyright 2008-2009  Hector Martin  <marcan@marcansoft.com>

#include "cache.h"
#include "dip.h"

void _memset(void *ptr, int c, u32 size)
{
	char* ptr2 = ptr;
	while(size--) *ptr2++ = c;
}

typedef struct _dolheader {
	u32 text_pos[7];
	u32 data_pos[11];
	u32 text_start[7];
	u32 data_start[11];
	u32 text_size[7];
	u32 data_size[11];
	u32 bss_start;
	u32 bss_size;
	u32 entry_point;
} dolheader;

int di_read(void *ptr, void *offset, u32 len)
{
	DI_STATUS	= 0x2A | 0x14;			// clear IRQs
	DI_CMD_0	= 0xA8000000;
	DI_CMD_1	= ((u32)offset)>>2;
	DI_CMD_2	= len;
	DI_DMA_ADR	= (u32)ptr;
	DI_DMA_LEN	= len;
	
	
//Start cmd!
	DI_CONTROL = 3;
	while( DI_CONTROL == 3 );
	while( DI_SCONTROL & 1 );

	while(1)
	{
		if( DI_SSTATUS & 0x04 )		//Error
		{
			DI_SSTATUS = 0;
			return 0;
		}
		if( DI_SSTATUS & 0x10 )		//Transfer done
		{
			sync_before_read( (void*)ptr, len );

			DI_SSTATUS = 0;
			return 1;
		}
	}
}

u32 entrypoint = 0;
void _main(void)
{
	/* start reading tgc */
	DI_CMD_0	= 0xFA000000;
	DI_CONTROL = 3;

	while( DI_CONTROL == 3 ) ;
	while( DI_SCONTROL & 1 ) ;

	/* read dol */
	u32 i;
	dolheader *dolfile = (dolheader *)0x93002EE0;
	sync_before_read(dolfile, 0x120);
	void *doloffset = (void*)(*(vu32*)0x93002FE0);

	entrypoint = dolfile->entry_point;

	for (i = 0; i < 7; i++)
	{
		if ((!dolfile->text_size[i]) || (dolfile->text_start[i] < 0x100))
			continue;
		di_read((void *) dolfile->text_start[i], doloffset + dolfile->text_pos[i], dolfile->text_size[i]);
	}

	for (i = 0; i < 11; i++)
	{
		if ((!dolfile->data_size[i]) || (dolfile->data_start[i] < 0x100))
			continue;
		di_read((void *) dolfile->data_start[i], doloffset + dolfile->data_pos[i], dolfile->data_size[i]);
	}

	asm volatile (
		"lis %r3, entrypoint@h\n"
		"ori %r3, %r3, entrypoint@l\n"
		"lwz %r3, 0(%r3)\n"
		"mtlr %r3\n"
		"blr\n"
	);
}
