/*
RealDI.h for Nintendont (Kernel)

Copyright (C) 2015 FIX94

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
#include "Config.h"
#include "common.h"
#include "string.h"
#include "alloc.h"
#include "RealDI.h"
#include "DI.h"

static struct ipcmessage realdimsg ALIGNED(32);
static u32 outbuf[8] __attribute__((aligned(32)));
static u32 spinup[8] __attribute__((aligned(32)));
static u32 identify[8] __attribute__((aligned(32)));
static u32 readdiscid[8] __attribute__((aligned(32)));
static const char di_path[] __attribute__((aligned(32))) = "/dev/di";
static s32 di_fd = -1;
u32 RealDiscCMD = 0, RealDiscError = 0;

//No ISO Cache so lets take alot of memory
static u8 *const DISC_FRONT_CACHE = (u8*)0x12000000;
static u8 *const DISC_DRIVE_BUFFER = (u8*)0x12000800;
static const u32 DISC_DRIVE_BUFFER_LENGTH = 0x7FF000;
static u8 *const DISC_TMP_CACHE = (u8*)0x127FF800;

static s32 realdiqueue = -1;
static vu32 realdi_msgrecv = 0;
u32 RealDI_Thread(void *arg)
{
	struct ipcmessage *msg = NULL;
	while(1)
	{
		mqueue_recv( realdiqueue, &msg, 0 );
		mqueue_ack( msg, 0 );
		realdi_msgrecv = 1;
	}
	return 0;
}

extern char __realdi_stack_addr, __realdi_stack_size;
static u32 RealDI_Thread_P = 0;
static u8 *realdiheap = NULL;

void RealDI_Init()
{
	memset32(spinup, 0, 32);
	spinup[0] = 0x8A000000;
	spinup[1] = 1;
	sync_after_write(spinup, 32);

	memset32(identify, 0, 32);
	identify[0] = 0x12000000;
	sync_after_write(identify, 32);

	memset32(readdiscid, 0, 32);
	readdiscid[0] = 0x70000000;
	sync_after_write(readdiscid, 32);

	//have real disc drive ready
	di_fd = IOS_Open(di_path, 0);

	realdiheap = (u8*)malloca(32,32);
	realdiqueue = mqueue_create(realdiheap, 1);

	RealDI_Thread_P = do_thread_create(RealDI_Thread, ((u32*)&__realdi_stack_addr), ((u32)(&__realdi_stack_size)), 0x50);
	thread_continue(RealDI_Thread_P);
	mdelay(100);

	RealDI_Identify(true);
}

void RealDI_Identify(bool NeedsGC)
{
	ClearRealDiscBuffer();
	u32 Length = 0x800;
	RealDiscCMD = DIP_CMD_NORMAL;
	const u8 * TmpBuf = ReadRealDisc(&Length, 0, false);
	if(IsGCGame((u32)TmpBuf) == false)
	{
		Length = 0x800;
		RealDiscCMD = DIP_CMD_DVDR;
		TmpBuf = ReadRealDisc(&Length, 0, false);
		if(IsGCGame((u32)TmpBuf) == false)
		{
			if(NeedsGC)
			{
				dbgprintf("No GC Disc!\r\n");
				BootStatusError(-2, -2);
				mdelay(4000);
				Shutdown();
			}
			else
				dbgprintf("No GC Disc, continuing anyways\r\n");
			return;
		}
	}
	memcpy((void*)0, TmpBuf, 0x20); //Disc Header
	sync_after_write((void*)0, 0x20);
	BI2region = read32((u32)(TmpBuf+0x458)); // BI2.bin region code
	dbgprintf("DI:Reading real disc with command 0x%02X\r\n", RealDiscCMD);
}

static vu32 WaitForWrite = 0, WaitForRead = 0;
void RealDI_Update()
{
	if(WaitForWrite == 1)
	{
		WaitForWrite = 0;
		while(WaitForRead == 0)
			udelay(20);
	}
	if(WaitForRead == 1 && (read32(DIP_CONTROL) & 1) == 0)
	{
		if(read32(DIP_STATUS) & 4)
			RealDiscError = 1;
		write32(DIP_STATUS, 0x54); //mask and clear interrupts
		udelay(70);
		WaitForRead = 0;
	}
}

static u32 switch_stat = 0;
bool RealDI_NewDisc()
{
	if(switch_stat == 0 && read32(DIP_COVER) == 4) //disc switch!
	{
		realdi_msgrecv = 0;
		switch_stat++;
		IOS_IoctlAsync(di_fd, 0x8A, spinup, 0x20, outbuf, 0x20, realdiqueue, &realdimsg);
	}
	if(realdi_msgrecv == 1)
	{
		realdi_msgrecv = 0;
		if(switch_stat == 1)
		{
			switch_stat++;
			IOS_IoctlAsync(di_fd, 0x12, identify, 0x20, outbuf, 0x20, realdiqueue, &realdimsg);
		}
		else if(switch_stat == 2)
		{
			switch_stat++;
			IOS_IoctlAsync(di_fd, 0x70, readdiscid, 0x20, outbuf, 0x20, realdiqueue, &realdimsg);
		}
		else if(switch_stat == 3)
		{
			switch_stat = 0; //done!
			return true;
		}
	}
	return false;
}

static u64 DVD_OFFSET64 = ~0;
void ClearRealDiscBuffer(void)
{
	DVD_OFFSET64 = ~0;
	memset32(DISC_DRIVE_BUFFER, 0, DISC_DRIVE_BUFFER_LENGTH);
	sync_after_write(DISC_DRIVE_BUFFER, DISC_DRIVE_BUFFER_LENGTH);
}

extern bool access_led;
extern u64 ISOShift64;
const u8 *ReadRealDisc(u32 *Length, u32 Offset, bool NeedSync)
{
	//dbgprintf("ReadRealDisc(%08x %08x)\r\n", *Length, Offset);

	u32 CachedBlockStart = 0;
	u32 ReadDiff = 0;

	u32 TmpLen = *Length;
	u64 TmpOffset = (u64)Offset + ISOShift64;

	if(RealDiscCMD == DIP_CMD_DVDR)
	{
		// Wii's disc drive can only read full ISO-9660
		// sectors when using standard DVD media. (2048 bytes)
		u64 AlignedOffset = ALIGN_BACKWARD(TmpOffset, 0x800);
		ReadDiff = (u32)(TmpOffset - AlignedOffset);
		if(AlignedOffset == DVD_OFFSET64)
		{
			sync_before_read(DISC_TMP_CACHE, 0x800);
			//dbgprintf("Using cached offset %08llx\r\n", DVD_OFFSET64>>11);
			memcpy(DISC_FRONT_CACHE, DISC_TMP_CACHE, 0x800);
			CachedBlockStart = 0x800;
			u32 AlignedLength = ALIGN_FORWARD(TmpLen + ReadDiff, 0x800);
			if( AlignedLength > 0 && AlignedLength == CachedBlockStart )
				return (DISC_FRONT_CACHE + ReadDiff);
		}
		//dbgprintf("ReadDiff: %08x\r\n", ReadDiff);
	}
	if(NeedSync)
	{
		WaitForWrite = 1;
		while(WaitForWrite == 1)
			udelay(20);
	}

	//turn on drive led
	if (access_led) set32(HW_GPIO_OUT, GPIO_SLOT_LED);

	if (TmpLen > DISC_DRIVE_BUFFER_LENGTH - ReadDiff)
	{
		TmpLen = DISC_DRIVE_BUFFER_LENGTH - ReadDiff;
		*Length = TmpLen;
		//dbgprintf("New Length: %08x\r\n", TmpLen);
	}

	write32(DIP_STATUS, 0x54); //mask and clear interrupts

	//Actually read
	if (RealDiscCMD == DIP_CMD_DVDR)
	{
		// Adjust length and offset for DVD-R mode.
		TmpLen = ALIGN_FORWARD(TmpLen + ReadDiff, 0x800) - CachedBlockStart;
		TmpOffset = ALIGN_BACKWARD(TmpOffset, 0x800) + CachedBlockStart;

		write32(DIP_CMD_0, DIP_CMD_DVDR << 24);
		write32(DIP_CMD_1, (u32)(TmpOffset >> 11));
		write32(DIP_CMD_2, TmpLen >> 11);
	}
	else
	{
		write32(DIP_CMD_0, DIP_CMD_NORMAL << 24);
		write32(DIP_CMD_1, (u32)(TmpOffset >> 2));
		write32(DIP_CMD_2, TmpLen);
	}

	//dbgprintf("Read %08x %08x\r\n", read32(DIP_CMD_1), read32(DIP_CMD_2));
	sync_before_read(DISC_DRIVE_BUFFER, TmpLen);
	write32(DIP_DMA_ADR, (u32)DISC_DRIVE_BUFFER);
	write32(DIP_DMA_LEN, TmpLen);

	write32( DIP_CONTROL, 3 );
	udelay(70);

	if(NeedSync)
	{
		WaitForRead = 1;
		while(WaitForRead == 1)
			udelay(200);
	}
	else
	{
		while(read32(DIP_CONTROL) & 1)
			udelay(200);
		write32(DIP_STATUS, 0x54); //mask and clear interrupts
		udelay(70);
	}

	//turn off drive led
	if (access_led) clear32(HW_GPIO_OUT, GPIO_SLOT_LED);

	if(RealDiscCMD == DIP_CMD_DVDR)
	{
		u64 LastBlockStart = ((u64)(read32(DIP_CMD_2)) - 1) << 11;
		DVD_OFFSET64 = ((u64)(read32(DIP_CMD_1)) << 11) + LastBlockStart;
		memcpy(DISC_TMP_CACHE, DISC_DRIVE_BUFFER + LastBlockStart, 0x800);
		sync_after_write(DISC_TMP_CACHE, 0x800);
		if(CachedBlockStart)
			return (DISC_FRONT_CACHE + ReadDiff);
	}

	return DISC_DRIVE_BUFFER + ReadDiff;
}
