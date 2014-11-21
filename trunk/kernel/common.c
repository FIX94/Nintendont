#include "string.h"
#include "syscalls.h"
#include "global.h"
#include "EXI.h"
#include "Config.h"
#include "debug.h"

void BootStatus(s32 Value, u32 secs, u32 scnt)
{
	//memset32( STATUS, Value, 0x20 );
	if(scnt)
	{
		float drvs = (float)scnt / 1024 * secs / 1024;
		if(drvs > 1024)
		{
			STATUS_GB_MB = 1; //drvs is in GB
			drvs /= 1024;
		} else
			STATUS_GB_MB = 0;	//drvs is in MB
		*(vu32*)(0x10004100 + 4) = scnt;
		STATUS_SECTOR = secs;
		STATUS_DRIVE = drvs;
	}
	STATUS_LOADING = Value;
	STATUS_ERROR = 0;		//clear error status
	sync_after_write(STATUS, 0x20);
}

void BootStatusError(s32 Value, s32 error)
{
	//memset32( STATUS, Value, 0x20 );
	STATUS_LOADING = Value;
	STATUS_ERROR = error;
	sync_after_write(STATUS, 0x20);
}

u16 bs16( u16 s )
{
	return (s << 8) | (s >> 8);
}
u32 bs32( u32 i )
{
	return (i<<24)|(i>>24)|((i&0xFF00)<<8)|((i>>8)&0xFF00);
}
/*
	Since Starlet can only access MEM1 via 32byte write and 8/16 writes
	cause unpredictable results we this code is needed.

	This automatically detects the misalignment and writes the value
	via two 32bit writes
*/
void W16(u32 Address, u16 Data)
{
	u32 Tmp = R32(Address);
	W32(Address, (Tmp & 0xFFFF) | (Data << 16));
}

void W32(u32 Address, u32 Data)
{
	if( Address & 3 )
	{
		//dbgprintf("[%08X] %08X\r\n",Address,Data);
		u32 valA = read32(Address & (~3));
		u32 valB = read32((Address + 3) & (~3));
		//dbgprintf("[%08X] %08X\r\n", Address & (~3), valA );
		//dbgprintf("[%08X] %08X\r\n", (Address+3) & (~3), valB );
		if((Address & 3) == 1)
		{
			valA &= 0xFF000000;
			valA |= Data>>8;
			write32(Address & (~3), valA);
			valB &= 0x00FFFFFF;
			valB |= (Data&0xFF)<<24;
			write32((Address + 3) & (~3), valB);
		}
		else if((Address & 3) == 2)
		{
			valA &= 0xFFFF0000;
			valA |= Data>>16;
			write32(Address & (~3), valA);
			valB &= 0x0000FFFF;
			valB |= (Data&0xFFFF)<<16;
			write32((Address + 3) & (~3), valB);
		}
		else
		{
			valA &= 0xFFFFFF00;
			valA |= Data>>24;
			write32(Address & (~3), valA);
			valB &= 0x000000FF;
			valB |= (Data&0xFFFFFF)<<8;
			write32((Address + 3) & (~3), valB);
		}
		//dbgprintf("[%08X] %08X\r\n", Address & (~3), valA );
		//dbgprintf("[%08X] %08X\r\n", (Address+3) & (~3), valB );
	}
	else
	{
		write32( Address, Data );
	}
}

u16 R16(u32 Address)
{
	return R32(Address) >> 16;
}

u32 R32(u32 Address)
{
	if(Address & 3)
	{
		u32 valA = read32(Address & (~3));
		u32 valB = read32((Address + 3) & (~3));
		if((Address & 3) == 1)
			return ((valA&0xFFFFFF)<<8) | (valB>>24);
		else if((Address & 3) == 2)
			return ((valA&0xFFFF)<<16) | (valB>>16);
		return ((valA&0xFF)<<24) | (valB>>8);
	}
	return read32(Address);
}

void udelay(int us)
{
	u8 heap[0x10];
	struct ipcmessage *msg = NULL;
	s32 mqueue = -1;
	s32 timer = -1;

	mqueue = mqueue_create(heap, 1);
	if(mqueue < 0)
		goto out;
	timer = TimerCreate(us, 0, mqueue, 0xbabababa);
	if(timer < 0)
		goto out;
	mqueue_recv(mqueue, &msg, 0);

out:
	if(timer >= 0)
		TimerDestroy(timer);
	if(mqueue >= 0)
		mqueue_destroy(mqueue);
}

void mdelay(int ms)
{
	udelay(ms * 1000);
}

void Shutdown( void )
{
	dbgprintf("Got Shutdown button call\n");
	if( ConfigGetConfig(NIN_CFG_MEMCARDEMU) )
		EXIShutdown();

#if 0

#ifdef DEBUG
	int i;
	//if( ConfigGetConfig(NIN_CFG_MEMEMU) )
	//{
	//	for( i = 0; i < 0x20; i+=4 )
	//		dbgprintf("0x%08X:0x%08X\t0x%08X\r\n", i, read32( EXI_BASE + i ), read32( EXI_SHADOW + i ) );
	//	dbgprintf("\r\n");
	//}

	for( i = 0; i < 0x30; i+=4 )
		dbgprintf("0x%08X:0x%08X\t0x%08X\r\n", i, read32( DI_BASE + i ), read32( DI_SHADOW + i ) );
	dbgprintf("\r\n");

	for( i = 0; i < 0x30; i+=4 )
		dbgprintf("0x%08X:0x%08X\t0x%08X\r\n", 0x0D806000 + i, read32( 0x0D806000 + i ), read32( 0x0D006000 + i ) );
#endif

#endif
	if( IsWiiU )
	{
		write32( 0x0D8005E0, 0xFFFFFFFE );

	} else {		
		set32( HW_GPIO_ENABLE, GPIO_SHUTDOWN );
		set32( HW_GPIO_OUT, GPIO_SHUTDOWN );
	}

	while(1);
}
void Asciify( char *str )
{
	int i=0;
	int length = strlen(str);
	for( i=0; i < length; i++ )
		if( str[i] < 0x20 || str[i] > 0x7F )
			str[i] = '_';
}
unsigned int atox( char *String )
{
	u32 val=1;
	u32 len=0;
	u32 i;

	while(val)
	{
		switch(String[len])
		{
			case 0x0a:
			case 0x0d:
			case 0x00:
			case ',':
				val = 0;
				len--;
				break;
		}
		len++;
	}

	for( i=0; i < len; ++i )
	{
		if( String[i] >= '0' && String[i] <='9' )
		{
			val |= (String[i]-'0') << (((len-1)-i) * 4);

		} else if( String[i] >= 'A' && String[i] <='Z' ) {

			val |= (String[i]-'7') << (((len-1)-i) * 4);

		} else if( String[i] >= 'a' && String[i] <='z' ) {

			val |= (String[i]-'W') << (((len-1)-i) * 4);
		}
	}

	return val;
}

void wait_for_ppc(u8 multi)
{
	udelay(45*multi);
}