#ifndef __GCAM_H__
#define __GCAM_H__

#include "string.h"
#include "global.h"
#include "alloc.h"
#include "ff.h"
#include "vsprintf.h"

#define		GCAM_BASE		0x13026100

#define		GCAM_CMD		(GCAM_BASE+0x00)
#define		GCAM_CMD_1		(GCAM_BASE+0x04)
#define		GCAM_CMD_2		(GCAM_BASE+0x08)
#define		GCAM_CMD_3		(GCAM_BASE+0x0C)
#define		GCAM_CMD_4		(GCAM_BASE+0x10)
#define		GCAM_RETURN		(GCAM_BASE+0x14)
#define		GCAM_CONTROL	(GCAM_BASE+0x18)
#define		GCAM_STATUS		(GCAM_BASE+0x1C)

#define		GCAM_SHADOW		(GCAM_BASE + 0x20)

#define		GCAM_SCMD		(GCAM_SHADOW+0x00)
#define		GCAM_SCMD_1		(GCAM_SHADOW+0x04)
#define		GCAM_SCMD_2		(GCAM_SHADOW+0x08)
#define		GCAM_SCMD_3		(GCAM_SHADOW+0x0C)
#define		GCAM_SCMD_4		(GCAM_SHADOW+0x10)
#define		GCAM_SRETURN	(GCAM_SHADOW+0x14)
#define		GCAM_SCONTROL	(GCAM_SHADOW+0x18)
#define		GCAM_SSTATUS	(GCAM_SHADOW+0x1C)

enum CARDCommands
{
	CARD_INIT			      = 0x10,
	CARD_GET_CARD_STATE	= 0x20,
	CARD_IS_PRESENT     = 0x40,
	CARD_LOAD_CARD	  	= 0xB0,
	CARD_CLEAN_CARD	  	= 0xA0,
	CARD_READ		  	    = 0x33,
	CARD_WRITE			    = 0x53,
	CARD_WRITE_INFO	    = 0x7C,
	CARD_78             = 0x78,
	CARD_7A             = 0x7A,
	CARD_7D			      	= 0x7D,
	CARD_D0			       	= 0xD0,
	CARD_80			      	= 0x80,
};

void GCAMInit( void );
void GCAMCARDCleanStatus( u32 status );
void GCAMUpdateRegisters( void );

#endif
