#ifndef __COMMON_H__
#define __COMMON_H__

#include "ff_utf8.h"

#define SEEK_CUR    1
#define SEEK_END    2
#define SEEK_SET    0

#define STATUS			((void*)0x10004100)
#define STATUS_LOADING	(*(vu32*)(0x10004100))
#define STATUS_SECTOR	(*(vu32*)(0x10004100 + 8))
#define STATUS_DRIVE	(*(float*)(0x1000410C))
#define STATUS_GB_MB	(*(vu32*)(0x10004100 + 16))
#define STATUS_ERROR	(*(vu32*)(0x10004100 + 20))

extern void *memset8( void *dst, int x, size_t len );
extern void *memset16( void *dst, int x, size_t len );
extern void *memset32( void *dst, int x, size_t len );
extern u64 read64( u32 addr );
extern void write64( u32 addr, u64 data );

void BootStatus(s32 Value, u32 secs, u32 scnt);
void BootStatusError(s32 Value, s32 error);
void udelay(int us);
void mdelay(int ms);

/**
 * Change non-printable characters in a string to '_'.
 * @param str String.
 */
void Asciify(char *str);

void Shutdown(void) NORETURN;

/*
 * Since Starlet can only access MEM1 via 32-bit write and 8/16 writes
 * cause unpredictable results, this code is needed.
 *
 * This automatically detects the misalignment and writes the value
 * via two 32bit writes
 */

u32 R32(u32 Address);
static inline u16 R16(u32 Address)
{
	return R32(Address) >> 16;
}
 
void W32(u32 Address, u32 Data);
static inline void W16(u32 Address, u16 Data)
{
	u32 Tmp = R32(Address);
	W32(Address, (Tmp & 0xFFFF) | (Data << 16));
}

void wait_for_ppc(u8 multi);
void InitCurrentTime();
u32 GetCurrentTime();

FRESULT f_open_main_drive(FIL* fp, const char* path, BYTE mode);
FRESULT f_open_secondary_drive(FIL* fp, const char* path, BYTE mode);
FRESULT f_mkdir_main_drive(const char* path);
FRESULT f_mkdir_secondary_drive(const char* path);

#endif
