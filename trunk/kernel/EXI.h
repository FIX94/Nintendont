#ifndef __EXI_H__
#define __EXI_H__
#include "global.h"

#ifdef EXIPATCH

enum EXIDevice
{
	EXI_DEV_NONE,
	EXI_DEV_MEMCARD_A,
	EXI_DEV_MEMCARD_B,
	EXI_DEV_MASK_ROM_RTC_SRAM_UART,
	EXI_DEV_AD16,
	EXI_DEV_SP1,
	EXI_DEV_ETH,
};

enum
{
	BBA_RECV_SIZE	= 0x800,
	BBA_MEM_SIZE	= 0x1000,

	CB_OFFSET	= 0x100,
	CB_SIZE		= (BBA_MEM_SIZE - CB_OFFSET),
	SIZEOF_ETH_HEADER		= 0xe,
	SIZEOF_RECV_DESCRIPTOR	= 4,
	
	EXI_DEVTYPE_ETHER	= 0x04020200,
};

enum EXICommands {
	MEM_READ_ID		= 1,
	MEM_READ_ID_NINTENDO,
	MEM_READ_STATUS,
	MEM_BLOCK_READ,
	MEM_BLOCK_WRITE,
	MEM_BLOCK_ERASE,
	MEM_FORMAT,

	SRAM_READ,
	SRAM_WRITE,

	IPL_READ_FONT,
};

#define IPL_ROM_FONT_SJIS               0x1AFF00
#define IPL_ROM_FONT_ANSI               0x1FCF00

#define MC_STATUS_BUSY					0x80
#define MC_STATUS_UNLOCKED				0x40
#define MC_STATUS_SLEEP					0x20
#define MC_STATUS_ERASEERROR			0x10
#define MC_STATUS_PROGRAMEERROR			0x08
#define MC_STATUS_READY					0x01
#define SIZE_TO_Mb (1024 * 8 * 16)
#define MC_HDR_SIZE 0xA000


#define EXI_READ		0
#define EXI_WRITE		1
#define EXI_READWRITE	2 

void EXIInit();
void EXIUpdateRegistersNEW( void );
void EXIShutdown( void );
void EXISaveCard(void);
bool EXICheckCard(void);
bool EXIReadFontFile(u8* Data, u32 Length);

#endif

#endif
