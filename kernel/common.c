#include "string.h"
#include "global.h"
#include "EXI.h"
#include "Config.h"
#include "debug.h"

//#include <ctype.h> //somehow broke in devkitARM r46
extern int isprint(int in); //only used definition anyways

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

/*
 * Since Starlet can only access MEM1 via 32byte write and 8/16 writes
 * cause unpredictable results we this code is needed.

 * This automatically detects the misalignment and writes the value
 * via two 32bit writes
 */

u32 R32(u32 Address)
{
	if ((Address & 3) == 0)
	{
		return read32(Address);
	}

	const u32 valA = read32(Address & (~3));
	const u32 valB = read32((Address + 3) & (~3));
	switch (Address & 3)
	{
		case 0:
		default:
			// Shouldn't happen...
			return valA;
		case 1:
			return ((valA&0xFFFFFF)<<8) | (valB>>24);
		case 2:
			return ((valA&0xFFFF)<<16) | (valB>>16);
		case 3:
			return ((valA&0xFF)<<24) | (valB>>8);
	}
}

void W32(u32 Address, u32 Data)
{
	if ((Address & 3) == 0)
	{
		write32(Address, Data);
		return;
	}

	u32 valA = read32(Address & (~3));
	u32 valB = read32((Address + 3) & (~3));
	switch (Address & 3)
	{
		case 0:
		default:
			// Shouldn't happen...
			write32(Address, Data);
			break;
		case 1:
			valA &= 0xFF000000;
			valA |= Data>>8;
			write32(Address & (~3), valA);
			valB &= 0x00FFFFFF;
			valB |= (Data&0xFF)<<24;
			write32((Address + 3) & (~3), valB);
			break;
		case 2:
			valA &= 0xFFFF0000;
			valA |= Data>>16;
			write32(Address & (~3), valA);
			valB &= 0x0000FFFF;
			valB |= (Data&0xFFFF)<<16;
			write32((Address + 3) & (~3), valB);
			break;
		case 3:
			valA &= 0xFFFFFF00;
			valA |= Data>>24;
			write32(Address & (~3), valA);
			valB &= 0x000000FF;
			valB |= (Data&0xFFFFFF)<<8;
			write32((Address + 3) & (~3), valB);
			break;
	}
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

static const char stm_imm[] ALIGNED(32) = "/dev/stm/immediate";
static u8 stm_in[0x20] ALIGNED(32);
static u8 stm_out[0x20] ALIGNED(32);

extern bool isWiiVC;
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
		dbgprintf("0x%08X:0x%08X\r\n", i, read32( DI_BASE + i ) );
	dbgprintf("\r\n");

	for( i = 0; i < 0x30; i+=4 )
		dbgprintf("0x%08X:0x%08X\t0x%08X\r\n", 0x0D806000 + i, read32( 0x0D806000 + i ), read32( 0x0D006000 + i ) );
#endif

#endif
	/* Allow all IOS IRQs again */
	write32(HW_IPC_ARMCTRL, 0x36);
	/* Send STM Shutdown Ioctl (works on Wii, vWii and Wii VC) */
	s32 stm_fd = IOS_Open(stm_imm, 0);
	IOS_Ioctl(stm_fd,0x2003,stm_in,0x20,stm_out,0x20);
	while(1) mdelay(100);
}

/**
 * Change non-printable characters in a string to '_'.
 * @param str String.
 */
void Asciify(char *str)
{
	for (; *str != 0; str++)
	{
		if (!isprint(*str))
			*str = '_';
	}
}

void wait_for_ppc(u8 multi)
{
	udelay(45*multi);
}

static u32 timeBase = 0, timeStarlet = 0, wrappedAround = 0;
void InitCurrentTime()
{
	//get current time since 1970 from loader
	timeBase = read32(RESET_STATUS+4);
	//make sure to start based on starlet time
	timeStarlet = read32(HW_TIMER);
	timeBase -= TicksToSecs(timeStarlet);
#ifdef DEBUG_TIME
	dbgprintf("Time:Start with %u seconds and %u ticks\r\n",timeBase,timeStarlet);
#endif
}

u32 GetCurrentTime()
{
	u32 curtime = read32(HW_TIMER);
	if(timeStarlet > curtime) /* Wrapped around, add full cycle to base */
	{
		//counter some inaccuracy, will be behind by 1 second every
		//19 wraps (nearly half a day, good enough)
		wrappedAround++;
		if(wrappedAround == 3)
		{
			timeBase += 2263;
			wrappedAround = 0;
		}
		else
			timeBase += 2262;
#ifdef DEBUG_TIME
		dbgprintf("Time:Wrap with %u seconds and %u ticks\r\n",timeBase,curtime);
#endif
	}
	timeStarlet = curtime;
	return timeBase + TicksToSecs(timeStarlet);
}
