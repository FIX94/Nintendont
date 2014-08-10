#ifndef __COMMON_H__
#define __COMMON_H__

#define SEEK_CUR    1
#define SEEK_END    2
#define SEEK_SET    0

extern void *memset8( void *dst, int x, size_t len );
extern void *memset16( void *dst, int x, size_t len );
extern void *memset32( void *dst, int x, size_t len );
extern u64 read64( u32 addr );
extern void write64( u32 addr, u64 data );

u16 bs16( u16 s );
u32 bs32( u32 i );

void BootStatus(s32 Value, u32 secs, u32 scnt);
void BootStatusError(s32 Value, s32 error);
void udelay(int us);
void mdelay(int ms);
void Asciify( char *str );
unsigned int atox( char *String );
void Shutdown( void );
void W16(u32 Address, u16 Data);
void W32(u32 Address, u32 Data);
u32 R32(u32 Address);
void wait_for_ppc(u8 multi);

#endif
