/*
PatchTimers.c for Nintendont (Kernel)

Copyright (C) 2014 - 2019 FIX94

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
#include "debug.h"
#include "common.h"
#include "string.h"
#include "PatchTimers.h"

extern u32 isKirby;

static bool write64A( u32 Offset, u64 Value, u64 CurrentValue )
{
	if( read64(Offset) != CurrentValue )
		return false;
	write64( Offset, Value );
	return true;
}

//arithmetic high for addi calls
static void W16AH(u32 Address, u32 Data)
{
	//see if high bit of lower 16bits are set
	bool needsAdd = !!(Data&(1<<15));
	Data>>=16; //shift out lower 16bits
	if(needsAdd) Data++; //add 1 if high bit was set
	W16(Address, Data); //write parsed value
}
//logical high for ori calls
#define W16H(Address, Data) W16(Address, (Data)>>16)
//low for addi and ori calls
#define W16L(Address, Data) W16(Address, (Data)&0xFFFF)

static u32 CheckFor( u32 Buf, u32 Val )
{
	u32 i = 4;
	while(1)
	{
		u32 CurVal = read32(Buf+i);
		if(CurVal == 0x4E800020)
			break;
		if((CurVal & 0xFC00FFFF) == Val)
			return Buf+i;
		i += 4;
		//kirby has 1 far pattern
		if(i > (isKirby ? 0x80 : 0x40))
			break;
	}
	return 0;
}
//Bus speed
#define U32_TIMER_CLOCK_BUS_GC		0x09a7ec80
#define U32_TIMER_CLOCK_BUS_WII		0x0e7be2c0

#define FLT_TIMER_CLOCK_BUS_GC		0x4d1a7ec8
#define FLT_TIMER_CLOCK_BUS_WII		0x4d67be2c

//Processor speed
#define U32_TIMER_CLOCK_CPU_GC		0x1cf7c580
#define U32_TIMER_CLOCK_CPU_WII		0x2b73a840
//WiiU with 5x CPU Multi
#define U32_TIMER_CLOCK_CPU_FAST	0x486b6dc0

#define FLT_TIMER_CLOCK_CPU_GC		0x4de7be2c
#define FLT_TIMER_CLOCK_CPU_WII		0x4e2dcea1
//WiiU with 5x CPU Multi
#define FLT_TIMER_CLOCK_CPU_FAST	0x4e90d6dc

//Ticks per second
#define U32_TIMER_CLOCK_SECS_GC		0x0269fb20
#define U32_TIMER_CLOCK_SECS_WII	0x039ef8b0

#define FLT_TIMER_CLOCK_SECS_GC		0x4c1a7ec8
#define FLT_TIMER_CLOCK_SECS_WII	0x4c67be2c

//Ticks per second divided by 60
#define U32_TIMER_CLOCK_60HZ_GC		0x000a4cb8
#define U32_TIMER_CLOCK_60HZ_WII	0x000f7314

//1 divided by Ticks per second
#define FLT_ONE_DIV_CLOCK_SECS_GC	0x32d418df
#define FLT_ONE_DIV_CLOCK_SECS_WII	0x328d65ea

//Ticks per millisecond
#define U32_TIMER_CLOCK_MSECS_GC	0x00009e34
#define U32_TIMER_CLOCK_MSECS_WII	0x0000ed4e

#define FLT_TIMER_CLOCK_MSECS_GC	0x471e3400
#define FLT_TIMER_CLOCK_MSECS_WII	0x476d4e00

//1 divided by Ticks per millisecond
#define FLT_ONE_DIV_CLOCK_MSECS_GC	0x37cf204a
#define FLT_ONE_DIV_CLOCK_MSECS_WII	0x378a1586

//RADTimerRead, Multiplier so (GC / 1.5)
#define U32_TIMER_CLOCK_RAD_GC		0xcf2049a1
#define U32_TIMER_CLOCK_RAD_WII		0x8a15866c

//1 divided by one 1200th of a second
#define FLT_ONE_DIV_CLOCK_1200_GC	0x37f88d25
#define FLT_ONE_DIV_CLOCK_1200_WII	0x37a5b36e

//osGetCount, Multiplier so (GC / 1.5f)
#define DBL_1_1574		0x3ff284b5dcc63f14ull
#define DBL_0_7716		0x3fe8b0f27bb2fec5ull

bool PatchTimers(u32 FirstVal, u32 Buffer, bool checkFloats)
{
	/* The floats in the data sections */
	if(checkFloats)
	{
		if( FirstVal == FLT_TIMER_CLOCK_BUS_GC )
		{
			write32(Buffer, FLT_TIMER_CLOCK_BUS_WII);
			dbgprintf("PatchTimers:[Timer Clock float Bus] applied (0x%08X)\r\n", Buffer );
			return true;
		}
		if( FirstVal == FLT_TIMER_CLOCK_CPU_GC )
		{
			if(IsWiiUFastCPU())
				write32(Buffer, FLT_TIMER_CLOCK_CPU_FAST);
			else
				write32(Buffer, FLT_TIMER_CLOCK_CPU_WII);
			dbgprintf("PatchTimers:[Timer Clock float CPU] applied (0x%08X)\r\n", Buffer );
			return true;
		}
		if( FirstVal == FLT_TIMER_CLOCK_SECS_GC )
		{
			write32(Buffer, FLT_TIMER_CLOCK_SECS_WII);
			dbgprintf("PatchTimers:[Timer Clock float s] applied (0x%08X)\r\n", Buffer );
			return true;
		}
		if( FirstVal == FLT_TIMER_CLOCK_MSECS_GC )
		{
			write32(Buffer, FLT_TIMER_CLOCK_MSECS_WII);
			dbgprintf("PatchTimers:[Timer Clock float ms] applied (0x%08X)\r\n", Buffer );
			return true;
		}
		if( FirstVal == FLT_ONE_DIV_CLOCK_SECS_GC )
		{
			write32(Buffer, FLT_ONE_DIV_CLOCK_SECS_WII);
			dbgprintf("PatchTimers:[Timer Clock float 1/s] applied (0x%08X)\r\n", Buffer );
			return true;
		}
		if( FirstVal == FLT_ONE_DIV_CLOCK_MSECS_GC )
		{
			write32(Buffer, FLT_ONE_DIV_CLOCK_MSECS_WII);
			dbgprintf("PatchTimers:[Timer Clock float 1/ms] applied (0x%08X)\r\n", Buffer );
			return true;
		}
		if( FirstVal == FLT_ONE_DIV_CLOCK_1200_GC )
		{
			write32(Buffer, FLT_ONE_DIV_CLOCK_1200_WII);
			dbgprintf("PatchTimers:[Timer Clock float 1/1200] applied (0x%08X)\r\n", Buffer );
			return true;
		}
	}
	/* For Nintendo Puzzle Collection */
	if( FirstVal == 0x38C00BB8 && (u32)Buffer == 0x770528 )
	{	//it IS a smooth 1.5 BUT the game is actually not properly timed, good job devs
		write32(Buffer, 0x38C01194);
		dbgprintf("PatchTimers:[Timer Clock Panel de Pon] applied (0x%08X)\r\n", Buffer );
		return true;
	}
	/* Coded in values */
	FirstVal &= 0xFC00FFFF;
	if( FirstVal == 0x3C0009A7 )
	{
		u32 NextP = CheckFor(Buffer, 0x6000EC80);
		if(NextP > 0)
		{
			W16H(Buffer + 2, U32_TIMER_CLOCK_BUS_WII);
			W16L(NextP + 2, U32_TIMER_CLOCK_BUS_WII);
			dbgprintf("PatchTimers:[Timer Clock ori Bus] applied (0x%08X)\r\n", Buffer );
			return true;
		}
	}
	if( FirstVal == 0x3C0009A8 )
	{
		u32 NextP = CheckFor(Buffer, 0x3800EC80);
		if(NextP > 0)
		{
			W16AH(Buffer + 2, U32_TIMER_CLOCK_BUS_WII);
			W16L(NextP + 2, U32_TIMER_CLOCK_BUS_WII);
			dbgprintf("PatchTimers:[Timer Clock addi Bus] applied (0x%08X)\r\n", Buffer );
			return true;
		}
	}
	if( FirstVal == 0x3C001CF7 )
	{
		u32 NextP = CheckFor(Buffer, 0x6000C580);
		if(NextP > 0)
		{
			if(IsWiiUFastCPU())
			{
				W16H(Buffer + 2, U32_TIMER_CLOCK_CPU_FAST);
				W16L(NextP + 2, U32_TIMER_CLOCK_CPU_FAST);
			}
			else
			{
				W16H(Buffer + 2, U32_TIMER_CLOCK_CPU_WII);
				W16L(NextP + 2, U32_TIMER_CLOCK_CPU_WII);
			}
			dbgprintf("PatchTimers:[Timer Clock ori CPU] applied (0x%08X)\r\n", Buffer );
			return true;
		}
	}
	if( FirstVal == 0x3C001CF8 )
	{
		u32 NextP = CheckFor(Buffer, 0x3800C580);
		if(NextP > 0)
		{
			if(IsWiiUFastCPU())
			{
				W16AH(Buffer + 2, U32_TIMER_CLOCK_CPU_FAST);
				W16L(NextP + 2, U32_TIMER_CLOCK_CPU_FAST);
			}
			else
			{
				W16AH(Buffer + 2, U32_TIMER_CLOCK_CPU_WII);
				W16L(NextP + 2, U32_TIMER_CLOCK_CPU_WII);
			}
			dbgprintf("PatchTimers:[Timer Clock addi CPU] applied (0x%08X)\r\n", Buffer );
			return true;
		}
	}
	if( FirstVal == 0x3C000269 )
	{
		u32 NextP = CheckFor(Buffer, 0x6000FB20);
		if(NextP > 0)
		{
			W16H(Buffer + 2, U32_TIMER_CLOCK_SECS_WII);
			W16L(NextP + 2, U32_TIMER_CLOCK_SECS_WII);
			dbgprintf("PatchTimers:[Timer Clock ori s] applied (0x%08X)\r\n", Buffer );
			return true;
		}
	}
	if( FirstVal == 0x3C00026A )
	{
		u32 NextP = CheckFor(Buffer, 0x3800FB20);
		if(NextP > 0)
		{
			W16AH(Buffer + 2, U32_TIMER_CLOCK_SECS_WII);
			W16L(NextP + 2, U32_TIMER_CLOCK_SECS_WII);
			dbgprintf("PatchTimers:[Timer Clock addi s] applied (0x%08X)\r\n", Buffer );
			return true;
		}
	}
	if( FirstVal == 0x3C00000A )
	{
		u32 NextP = CheckFor(Buffer, 0x60004CB8);
		if(NextP > 0)
		{
			W16H(Buffer + 2, U32_TIMER_CLOCK_60HZ_WII);
			W16L(NextP + 2, U32_TIMER_CLOCK_60HZ_WII);
			dbgprintf("PatchTimers:[Timer Clock ori 60Hz] applied (0x%08X)\r\n", Buffer );
			return true;
		}
	}
	if( FirstVal == 0x3C00000A )
	{
		u32 NextP = CheckFor(Buffer, 0x38004CB8);
		if(NextP > 0)
		{
			W16AH(Buffer + 2, U32_TIMER_CLOCK_60HZ_WII);
			W16L(NextP + 2, U32_TIMER_CLOCK_60HZ_WII);
			dbgprintf("PatchTimers:[Timer Clock addi 60Hz] applied (0x%08X)\r\n", Buffer );
			return true;
		}
	}
	if( FirstVal == 0x38000000 )
	{
		u32 NextP = CheckFor(Buffer, 0x60009E34);
		if(NextP > 0)
		{
			W16H(Buffer + 2, U32_TIMER_CLOCK_MSECS_WII);
			W16L(NextP + 2, U32_TIMER_CLOCK_MSECS_WII);
			dbgprintf("PatchTimers:[Timer Clock ori ms] applied (0x%08X)\r\n", Buffer );
			return true;
		}
	}
	if( FirstVal == 0x3C000001 )
	{
		u32 NextP = CheckFor(Buffer, 0x38009E34);
		if(NextP > 0)
		{
			W16AH(Buffer + 2, U32_TIMER_CLOCK_MSECS_WII);
			W16L(NextP + 2, U32_TIMER_CLOCK_MSECS_WII);
			dbgprintf("PatchTimers:[Timer Clock addi ms] applied (0x%08X)\r\n", Buffer );
			return true;
		}
	}
	/* RADTimerRead split into subparts */
	if( FirstVal == 0x3C000001 )
	{
		u32 NextP = CheckFor(Buffer, 0x60009E40);
		if(NextP > 0)
		{
			u32 smallTimer = U32_TIMER_CLOCK_RAD_WII >> 15;
			W16H(Buffer + 2, smallTimer);
			W16L(NextP + 2, smallTimer);
			dbgprintf("PatchTimers:[RADTimerRead ori shift] applied (0x%08X)\r\n", Buffer );
			return true;
		}
	}
	if( FirstVal == 0x3C00CF20 )
	{
		u32 NextP = CheckFor(Buffer, 0x600049A1);
		if(NextP > 0)
		{
			W16H(Buffer + 2, U32_TIMER_CLOCK_RAD_WII);
			W16L(NextP + 2, U32_TIMER_CLOCK_RAD_WII);
			dbgprintf("PatchTimers:[RADTimerRead ori full] applied (0x%08X)\r\n", Buffer );
			return true;
		}
	}
	if( FirstVal == 0x3C00CF20 )
	{
		u32 NextP = CheckFor(Buffer, 0x380049A1);
		if(NextP > 0)
		{
			W16AH(Buffer + 2, U32_TIMER_CLOCK_RAD_WII);
			W16L(NextP + 2, U32_TIMER_CLOCK_RAD_WII);
			dbgprintf("PatchTimers:[RADTimerRead addi full] applied (0x%08X)\r\n", Buffer );
			return true;
		}
	}
	//kirby has many hardcoded timers, heres some dynamic ones,
	//there is also an 80 second timeout I cant patch because its
	//result is literally out of range for a 32bit integer
	if(isKirby)
	{
		if( FirstVal == 0x3C00003E)
		{
			u32 NextP = CheckFor(Buffer, 0x3800CC50);
			if(NextP > 0)
			{
				W16AH(Buffer + 2, U32_TIMER_CLOCK_SECS_WII/10);
				W16L(NextP + 2, U32_TIMER_CLOCK_SECS_WII/10);
				dbgprintf("PatchTimers:[Kirby addi 10Hz] applied (0x%08X)\r\n", Buffer );
				return true;
			}
		}
		if( FirstVal == 0x3C00001F)
		{
			u32 NextP = CheckFor(Buffer, 0x3800E628);
			if(NextP > 0)
			{
				W16AH(Buffer + 2, U32_TIMER_CLOCK_SECS_WII/20);
				W16L(NextP + 2, U32_TIMER_CLOCK_SECS_WII/20);
				dbgprintf("PatchTimers:[Kirby addi 20Hz] applied (0x%08X)\r\n", Buffer );
				return true;
			}
		}
		if( FirstVal == 0x3C00073E)
		{
			u32 NextP = CheckFor(Buffer, 0x3800F160);
			if(NextP > 0)
			{
				W16AH(Buffer + 2, U32_TIMER_CLOCK_SECS_WII*3);
				W16L(NextP + 2, U32_TIMER_CLOCK_SECS_WII*3);
				dbgprintf("PatchTimers:[Kirby addi 3 secs] applied (0x%08X)\r\n", Buffer );
				return true;
			}
		}
		if( FirstVal == 0x3C000C12)
		{
			u32 NextP = CheckFor(Buffer, 0x3800E7A0);
			if(NextP > 0)
			{
				W16AH(Buffer + 2, U32_TIMER_CLOCK_SECS_WII*5);
				W16L(NextP + 2, U32_TIMER_CLOCK_SECS_WII*5);
				dbgprintf("PatchTimers:[Kirby addi 5 secs] applied (0x%08X)\r\n", Buffer );
				return true;
			}
		}
	}
	return false;
}

//Audio sample rate differs from GC to Wii, affects some emus

//Sonic Mega Collection
//GC Original, (44100(emu audio)/60(emu video)) / (32029(gc audio)/59.94(gc video)) = 1.3755
static u32 smc_arr_gc[11] = 
{
	0x3FB00B44, 0x3FB00C4A, 0x3FB00D50, 0x3FB00E56, 0x3FB00F5C,
	0x3FB01062, //base value of 1.3755, rest goes up/down in 0.00003125 steps
	0x3FB01168, 0x3FB0126F, 0x3FB01375, 0x3FB0147B, 0x3FB01581,
};

//Wii Modified, (44100(emu audio)/60(emu video)) / (32000(wii audio)/59.94(wii video)) = 1.3767469
static u32 smc_arr_wii[11] = 
{
	0x3FB0341F, 0x3FB03525, 0x3FB0362C, 0x3FB03732, 0x3FB03838,
	0x3FB0393E, //base value of 1.3767469, rest goes up/down in 0.00003125 steps
	0x3FB03A44, 0x3FB03B4A, 0x3FB03C50, 0x3FB03D57, 0x3FB03E5D,
};

//Wii Modified PAL50, (44100(emu audio)/50(emu video)) / (32000(wii audio)/50(wii video)) = 1.378125
static u32 smc_pal50_arr_wii[11] = 
{
	0x3FB05C29, 0x3FB05E35, 0x3FB06042, 0x3FB0624E, 0x3FB0645A,
	0x3FB06666, //base value of 1.378125, rest goes up/down in 0.0000625 steps
	0x3FB06873, 0x3FB06A7F, 0x3FB06C8B, 0x3FB06E98, 0x3FB070A4,
};

void PatchStaticTimers()
{
	sync_before_read((void*)0xE0, 0x20);
	write32(0xF8, U32_TIMER_CLOCK_BUS_WII);
	if(IsWiiUFastCPU())
		write32(0xFC, U32_TIMER_CLOCK_CPU_FAST);
	else
		write32(0xFC, U32_TIMER_CLOCK_CPU_WII);
	sync_after_write((void*)0xE0, 0x20);
	if(write64A(0x001463E0, DBL_0_7716, DBL_1_1574))
	{
		#ifdef DEBUG_PATCH
		dbgprintf("PatchTimers:[Majoras Mask NTSC-J] applied\r\n");
		#endif
	}
	else if(write64A(0x00141C00, DBL_0_7716, DBL_1_1574))
	{
		#ifdef DEBUG_PATCH
		dbgprintf("PatchTimers:[Majoras Mask NTSC-U] applied\r\n");
		#endif
	}
	else if(write64A(0x00130860, DBL_0_7716, DBL_1_1574))
	{
		#ifdef DEBUG_PATCH
		dbgprintf("PatchTimers:[Majoras Mask PAL] applied\r\n");
		#endif
	}
	else if(read32(0x55A0) == 0x3C60000A && read32(0x55A4) == 0x38036000 
		&& read32(0x55B0) == 0x380000FF)
	{	/* The game uses 2 different timers */
		write32(0x55A0, 0x3C60000F); //lis r3, 0xF
		write32(0x55A4, 0x60609060); //ori r0, r3, 0x9060
		write32(0x55B0, 0x38000180); //li r0, 0x180
		/* Values get set twice */
		write32(0x5ECC, 0x3C60000F); //lis r3, 0xF
		write32(0x5ED4, 0x60609060); //ori r0, r3, 0x9060
		write32(0x5ED8, 0x38600180); //li r3, 0x180
		#ifdef DEBUG_PATCH
		dbgprintf("PatchTimers:[GT Cube NTSC-J] applied\r\n");
		#endif
	}
	else if(memcmp((void*)0x263130, smc_arr_gc, sizeof(smc_arr_gc)) == 0)
	{
		memcpy((void*)0x263130, smc_arr_wii, sizeof(smc_arr_wii));
		write32(0x3FF368, 0x3FB0393E); //base value of 1.3767469 again
		#ifdef DEBUG_PATCH
		dbgprintf("PatchTimers:[Sonic Mega Collection NTSC-U] applied\r\n");
		#endif
	}
	else if(memcmp((void*)0x264B08, smc_arr_gc, sizeof(smc_arr_gc)) == 0)
	{
		memcpy((void*)0x264B08, smc_arr_wii, sizeof(smc_arr_wii));
		write32(0x401388, 0x3FB0393E); //base value of 1.3767469 again
		#ifdef DEBUG_PATCH
		dbgprintf("PatchTimers:[Sonic Mega Collection NTSC-J] applied\r\n");
		#endif
	}
	else if(memcmp((void*)0x281508, smc_arr_gc, sizeof(smc_arr_gc)) == 0)
	{
		memcpy((void*)0x281508, smc_arr_wii, sizeof(smc_arr_wii));
		memcpy((void*)0x281534, smc_pal50_arr_wii, sizeof(smc_pal50_arr_wii));
		write32(0x41FCE0, 0x3FB0393E); //base value of 1.3767469 again
		#ifdef DEBUG_PATCH
		dbgprintf("PatchTimers:[Sonic Mega Collection PAL] applied\r\n");
		#endif
	}
	else if(read32(0x3F2DE0) == 0x3FB0147B)
	{
		//proto didnt have dynamic resample yet
		write32(0x3F2DE0, 0x3FB0393E); //base value of 1.3767469
		#ifdef DEBUG_PATCH
		dbgprintf("PatchTimers:[Sonic Mega Collection Proto NTSC-U] applied\r\n");
		#endif
	}
}
