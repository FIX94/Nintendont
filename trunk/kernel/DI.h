#ifndef __DI_H__
#define __DI_H__

#include "global.h"
#include "ff.h"

#define		DI_BASE		0x13026000

#define		DI_STATUS	(DI_BASE+0x00)
#define		DI_COVER	(DI_BASE+0x04)
#define		DI_CMD_0	(DI_BASE+0x08)
#define		DI_CMD_1	(DI_BASE+0x0C)
#define		DI_CMD_2	(DI_BASE+0x10)
#define		DI_DMA_ADR	(DI_BASE+0x14)
#define		DI_DMA_LEN	(DI_BASE+0x18)
#define		DI_CONTROL	(DI_BASE+0x1C)
#define		DI_IMM		(DI_BASE+0x20)
#define		DI_CONFIG	(DI_BASE+0x24)

#define		DI_SHADOW	(DI_BASE + 0x30)

#define		DI_SSTATUS	(DI_SHADOW+0x00)
#define		DI_SCOVER	(DI_SHADOW+0x04)
#define		DI_SCMD_0	(DI_SHADOW+0x08)
#define		DI_SCMD_1	(DI_SHADOW+0x0C)
#define		DI_SCMD_2	(DI_SHADOW+0x10)
#define		DI_SDMA_ADR	(DI_SHADOW+0x14)
#define		DI_SDMA_LEN	(DI_SHADOW+0x18)
#define		DI_SCONTROL	(DI_SHADOW+0x1C)
#define		DI_SIMM		(DI_SHADOW+0x20)
#define		DI_SCONFIG	(DI_SHADOW+0x24)

#define		DIP_BASE	0x0D806000

#define		DIP_STATUS	(DIP_BASE+0x00)
#define		DIP_COVER	(DIP_BASE+0x04)
#define		DIP_CMD_0	(DIP_BASE+0x08)
#define		DIP_CMD_1	(DIP_BASE+0x0C)
#define		DIP_CMD_2	(DIP_BASE+0x10)
#define		DIP_DMA_ADR	(DIP_BASE+0x14)
#define		DIP_DMA_LEN	(DIP_BASE+0x18)
#define		DIP_CONTROL	(DIP_BASE+0x1C)
#define		DIP_IMM		(DIP_BASE+0x20)
#define		DIP_CONFIG	(DIP_BASE+0x24)

#define		DIP_CMD_NORMAL	0xA8
#define		DIP_CMD_DVDR	0xD0

#define		DMA_READ		3
#define		IMM_READ		1

enum GameRegion
{
	JAP=0,
	USA,
	EUR,
	KOR,
	ASN,
	LTN,
	UNK,
	ALL,
};

extern u8 *DI_READ_BUFFER;
extern u32 DI_READ_BUFFER_LENGTH;

extern vu32 SDisInit;
extern u32 DiscChangeIRQ;
extern s32 DI_Handle;

void DIinit( bool FirstTime );
void DIInterrupt();
void DIRegister(void);
void DIUnregister(void);
void DIFinishAsync(void);
u32 DIReadThread(void *arg);
bool DiscCheckAsync( void );
void DiscReadSync(u32 Buffer, u32 Offset, u32 Length, u32 Mode);
bool DIChangeDisc( u32 DiscNumber );
void DIUpdateRegisters( void );
void DIReload(void);
bool DICheckTGC(u32 Buffer, u32 Length);
#endif