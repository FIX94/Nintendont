/*-----------------------------------------------------------------------
/  Low level disk interface modlue include file  R0.07   (C)ChaN, 2009
/-----------------------------------------------------------------------
/ FatFs module is an open source project to implement FAT file system to small
/ embedded systems. It is opened for education, research and development under
/ license policy of following trems.
/
/  Copyright (C) 2009, ChaN, all right reserved.
/
/ * The FatFs module is a free software and there is no warranty.
/ * You can use, modify and/or redistribute it for personal, non-profit or
/   commercial use without any restriction under your responsibility.
/ * Redistributions of source code must retain the above copyright notice.
/
/----------------------------------------------------------------------------*/
// original source: http://elm-chan.org/fsw/ff/00index_e.html

#ifndef _DISKIO

#include "integer.h"

/* Status of Disk Functions */
typedef BYTE	DSTATUS;

/* Results of Disk Functions */
typedef enum {
	RES_OK = 0,		/* 0: Successful */
	RES_ERROR,		/* 1: R/W Error */
	RES_WRPRT,		/* 2: Write Protected */
	RES_NOTRDY,		/* 3: Not Ready */
	RES_PARERR		/* 4: Invalid Parameter */
} DRESULT;


/*---------------------------------------*/
/* Prototypes for disk control functions */

void SetDiskFunctions(DWORD);

typedef DSTATUS (*DiskInitFunc)(BYTE, WORD*);
typedef DRESULT (*DiskReadFunc)(BYTE, BYTE*, DWORD, BYTE);
typedef DRESULT (*DiskWriteFunc)(BYTE, const BYTE*, DWORD, BYTE);

extern DiskInitFunc disk_initialize;
extern DiskReadFunc disk_read;
extern DiskWriteFunc disk_write;

DSTATUS disk_status(BYTE);
DWORD get_fattime(void);

/* Disk Status Bits (DSTATUS) */

#define STA_NOINIT		0x01	/* Drive not initialized */
#define STA_NODISK		0x02	/* No medium in the drive */
#define STA_PROTECT		0x04	/* Write protected */

#define _DISKIO
#endif
