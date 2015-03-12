/*
PatchTimers.c for Nintendont (Kernel)

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
#include "debug.h"
#include "common.h"
#include "string.h"
#include "PatchTimers.h"

static bool write64A( u32 Offset, u64 Value, u64 CurrentValue )
{
	if( read64(Offset) != CurrentValue )
		return false;
	write64( Offset, Value );
	return true;
}

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
		if(i > 0x40)
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

#define FLT_TIMER_CLOCK_CPU_GC		0x4de7be2c
#define FLT_TIMER_CLOCK_CPU_WII		0x4e2dcea1

//Ticks per second
#define U32_TIMER_CLOCK_SECS_GC		0x0269fb20
#define U32_TIMER_CLOCK_SECS_WII	0x039ef8b0

#define FLT_TIMER_CLOCK_SECS_GC		0x4c1a7ec8
#define FLT_TIMER_CLOCK_SECS_WII	0x4c67be2c

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

//osGetCount, Multiplier so (GC / 1.5f)
#define DBL_1_1574		0x3ff284b5dcc63f14ull
#define DBL_0_7716		0x3fe8b0f27bb2fec5ull

bool PatchTimers(u32 FirstVal, u32 Buffer)
{
	/* The floats in the data sections */
	if( FirstVal == FLT_TIMER_CLOCK_BUS_GC )
	{
		write32(Buffer, FLT_TIMER_CLOCK_BUS_WII);
		dbgprintf("Patch:[Timer Clock float Bus] applied (0x%08X)\r\n", Buffer );
		return true;
	}
	if( FirstVal == FLT_TIMER_CLOCK_CPU_GC )
	{
		write32(Buffer, FLT_TIMER_CLOCK_CPU_WII);
		dbgprintf("Patch:[Timer Clock float CPU] applied (0x%08X)\r\n", Buffer );
		return true;
	}
	if( FirstVal == FLT_TIMER_CLOCK_SECS_GC )
	{
		write32(Buffer, FLT_TIMER_CLOCK_SECS_WII);
		dbgprintf("Patch:[Timer Clock float s] applied (0x%08X)\r\n", Buffer );
		return true;
	}
	if( FirstVal == FLT_TIMER_CLOCK_MSECS_GC )
	{
		write32(Buffer, FLT_TIMER_CLOCK_MSECS_WII);
		dbgprintf("Patch:[Timer Clock float ms] applied (0x%08X)\r\n", Buffer );
		return true;
	}
	if( FirstVal == FLT_ONE_DIV_CLOCK_SECS_GC )
	{
		write32(Buffer, FLT_ONE_DIV_CLOCK_SECS_WII);
		dbgprintf("Patch:[Timer Clock float 1/s] applied (0x%08X)\r\n", Buffer );
		return true;
	}
	if( FirstVal == FLT_ONE_DIV_CLOCK_MSECS_GC )
	{
		write32(Buffer, FLT_ONE_DIV_CLOCK_MSECS_WII);
		dbgprintf("Patch:[Timer Clock float 1/ms] applied (0x%08X)\r\n", Buffer );
		return true;
	}
	/* RADTimerRead split into subparts */
	if( FirstVal == 0x3C000001 && read32(Buffer + 4) == 0x60009E40 )
	{
		u32 smallTimer = U32_TIMER_CLOCK_RAD_WII >> 15;
		W16(Buffer + 2, smallTimer >> 16);
		W16(Buffer + 6, smallTimer & 0xFFFF);
		dbgprintf("Patch:[RADTimerRead A] applied (0x%08X)\r\n", Buffer );
		return true;
	}
	if( FirstVal == 0x3CC0CF20 && read32(Buffer + 8) == 0x60C649A1 )
	{
		W16(Buffer + 2, U32_TIMER_CLOCK_RAD_WII >> 16);
		W16(Buffer + 10, U32_TIMER_CLOCK_RAD_WII & 0xFFFF);
		dbgprintf("Patch:[RADTimerRead B] applied (0x%08X)\r\n", Buffer );
		return true;
	}
	/* Coded in values */
	FirstVal &= 0xFC00FFFF;
	if( FirstVal == 0x3C001CF7 )
	{
		u32 NextP = CheckFor(Buffer, 0x6000C580);
		if(NextP > 0)
		{
			W16(Buffer + 2, U32_TIMER_CLOCK_CPU_WII >> 16);
			W16(NextP + 2, U32_TIMER_CLOCK_CPU_WII & 0xFFFF);
			dbgprintf("Patch:[Timer Clock ori CPU] applied (0x%08X)\r\n", Buffer );
			return true;
		}
	}
	if( FirstVal == 0x3C001CF8 )
	{
		u32 NextP = CheckFor(Buffer, 0x3800C580);
		if(NextP > 0)
		{
			W16(Buffer + 2, (U32_TIMER_CLOCK_CPU_WII >> 16) + 1);
			W16(NextP + 2, U32_TIMER_CLOCK_CPU_WII & 0xFFFF);
			dbgprintf("Patch:[Timer Clock addi CPU] applied (0x%08X)\r\n", Buffer );
			return true;
		}
	}
	if( FirstVal == 0x3C000269 )
	{
		u32 NextP = CheckFor(Buffer, 0x6000FB20);
		if(NextP > 0)
		{
			W16(Buffer + 2, U32_TIMER_CLOCK_SECS_WII >> 16);
			W16(NextP + 2, U32_TIMER_CLOCK_SECS_WII & 0xFFFF);
			dbgprintf("Patch:[Timer Clock ori s] applied (0x%08X)\r\n", Buffer );
			return true;
		}
	}
	if( FirstVal == 0x3C00026A )
	{
		u32 NextP = CheckFor(Buffer, 0x3800FB20);
		if(NextP > 0)
		{
			W16(Buffer + 2, (U32_TIMER_CLOCK_SECS_WII >> 16) + 1);
			W16(NextP + 2, U32_TIMER_CLOCK_SECS_WII & 0xFFFF);
			dbgprintf("Patch:[Timer Clock addi s] applied (0x%08X)\r\n", Buffer );
			return true;
		}
	}
	if( FirstVal == 0x38000000 )
	{
		u32 NextP = CheckFor(Buffer, 0x60009E34);
		if(NextP > 0)
		{
			W16(Buffer + 2, U32_TIMER_CLOCK_MSECS_WII >> 16);
			W16(NextP + 2, U32_TIMER_CLOCK_MSECS_WII & 0xFFFF);
			dbgprintf("Patch:[Timer Clock ori ms] applied (0x%08X)\r\n", Buffer );
			return true;
		}
	}
	if( FirstVal == 0x3C000001 )
	{
		u32 NextP = CheckFor(Buffer, 0x38009E34);
		if(NextP > 0)
		{
			W16(Buffer + 2, (U32_TIMER_CLOCK_MSECS_WII >> 16) + 1);
			W16(NextP + 2, U32_TIMER_CLOCK_MSECS_WII & 0xFFFF);
			dbgprintf("Patch:[Timer Clock addi ms] applied (0x%08X)\r\n", Buffer );
			return true;
		}
	}
	return false;
}

void PatchStaticTimers()
{
	sync_before_read((void*)0xE0, 0x20);
	write32(0xF8, U32_TIMER_CLOCK_BUS_WII);
	write32(0xFC, U32_TIMER_CLOCK_CPU_WII);
	sync_after_write((void*)0xE0, 0x20);
	if(write64A(0x001463E0, DBL_0_7716, DBL_1_1574))
	{
		#ifdef DEBUG_PATCH
		dbgprintf("Patch:[Majoras Mask NTSC-J] applied\r\n");
		#endif
	}
	else if(write64A(0x00141C00, DBL_0_7716, DBL_1_1574))
	{
		#ifdef DEBUG_PATCH
		dbgprintf("Patch:[Majoras Mask NTSC-U] applied\r\n");
		#endif
	}
	else if(write64A(0x00130860, DBL_0_7716, DBL_1_1574))
	{
		#ifdef DEBUG_PATCH
		dbgprintf("Patch:[Majoras Mask PAL] applied\r\n");
		#endif
	}
	else if(read32(0x1E71AC) == 0x3CE0000A && read32(0x1E71B8) == 0x39074CB8)
	{
		write32(0x1E71AC, 0x3CE0000F);
		write32(0x1E71B8, 0x39077314);
		#ifdef DEBUG_PATCH
		dbgprintf("Patch:[Killer7 PAL] applied\r\n");
		#endif
	}
}
