/*
apploader.c for Nintendont (Loader)

Copyright (C) 2014 FIX94

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
#include "cache.h"
#include "utils.h"
#include "apploader.h"
#include "dip.h"

typedef int   (*app_main)(char **dst, u32 *size, u32 *offset);
typedef void  (*app_init)(int (*report)(const char *fmt, ...));
typedef void *(*app_final)();
typedef void  (*app_entry)(void (**init)(int (*report)(const char *fmt, ...)), int (**main)(), void *(**final)());

static u8 *appldr = (u8*)0x81200000;
static struct _TGCInfo *TGCInfo = (struct _TGCInfo*)0x930031E0;

#define APPLDR_OFFSET	0x2440
#define APPLDR_CODE		0x2460

static int noprintf( const char *str, ... ) { return 0; };

void PrepareTGC(u32 Offset)
{
	if(TGCInfo->isTGC == 0)
		return;

	if(Offset == TGCInfo->doloffset || Offset == TGCInfo->fstoffset)
		TGCInfo->tgcoffset = 0; /* we can stop the manual offset correction */
}

void ParseTGC(char *Data, u32 Length, u32 Offset)
{
	if(TGCInfo->isTGC == 0)
		return;

	if(Offset == 0x420) /* update internal gcm header with the tgc header */
	{
		*(vu32*)(Data+0x00) = TGCInfo->doloffset;
		*(vu32*)(Data+0x04) = TGCInfo->fstoffset;
		*(vu32*)(Data+0x08) = TGCInfo->fstsize;
		*(vu32*)(Data+0x0C) = TGCInfo->fstsize;
		*(vu32*)(Data+0x34) = TGCInfo->userpos;
		*(vu32*)(Data+0x38) = 0x57058000L - (TGCInfo->userpos);
		sync_after_write(Data, Length);
	}
	else if(Offset == TGCInfo->fstoffset) /* update internal gcm fst with correct tgc positions */
	{
		char *FSTable = Data;
		u32 FSTEnt = *(u32*)(FSTable+0x08);
		FST *fst = (FST *)(FSTable);
		u32 i;
		for(i = 1; i < FSTEnt; ++i)
		{
			if(fst[i].Type == 0) /* update file offset */
				fst[i].FileOffset -= TGCInfo->fstupdate;
		}
		sync_after_write(Data, Length);
	}
}

/* Thanks Tinyload */
static struct
{
	char revision[16];
	void *entry;
	s32 size;
	s32 trailersize;
	s32 padding;
} apploader_hdr ALIGNED(32);

u32 Apploader_Run()
{
	app_entry appldr_entry;
	app_init  appldr_init;
	app_main  appldr_main;
	app_final appldr_final;

	char *dst;
	u32 len, offset;

	sync_before_read(TGCInfo, sizeof(struct _TGCInfo));

	/* Read apploader header */
	DVDLowRead(&apploader_hdr, 0x20, TGCInfo->tgcoffset + APPLDR_OFFSET);

	/* Calculate apploader length */
	u32 appldr_len = apploader_hdr.size + apploader_hdr.trailersize;

	/* Read apploader code */
	DVDLowRead(appldr, appldr_len, TGCInfo->tgcoffset + APPLDR_CODE);

	/* Flush into memory */
	sync_before_exec(appldr, appldr_len);

	/* Set basic information */
	*(vu32*)0x800000F8 = 243000000;				// Bus Clock Speed
	*(vu32*)0x800000FC = 729000000;				// CPU Clock Speed

	/* Set apploader entry function */
	appldr_entry = apploader_hdr.entry;

	/* Call apploader entry */
	#pragma GCC diagnostic warning "-Wincompatible-pointer-types"
	appldr_entry(&appldr_init, &appldr_main, &appldr_final);

	/* Initialize apploader */
	appldr_init(noprintf);

	while(appldr_main(&dst, &len, &offset))
	{
		/* Read data from DVD */
		PrepareTGC( offset );
		DVDLowRead( dst, len, offset + TGCInfo->tgcoffset );
		ParseTGC( dst, len, offset );
	}

	/* Set entry point from apploader */
	return (u32)appldr_final();
}
