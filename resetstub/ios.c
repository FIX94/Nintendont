/*
	TinyLoad - a simple region free (original) game launcher in 4k

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

/* This code comes from HBC's stub which was based on the Twilight Hack code */
// Copyright 2008-2009  Segher Boessenkool  <segher@kernel.crashing.org>
// Copyright 2008-2009  Andre Heider  <dhewg@wiibrew.org>
// Copyright 2008-2009  Hector Martin  <marcan@marcansoft.com>

#include "ios.h"
#include "cache.h"
#include "utils.h"

#define virt_to_phys(x) ((u32*)(((u32)(x))&0x3FFFFFFF))
#define phys_to_virt(x) ((u32*)(((u32)(x))|0x80000000))

// Low-level IPC access.

static inline u32
iread32(u32 addr)
{
	u32 x;

	asm volatile("lwz %0,0(%1) ; sync ; isync" : "=r"(x) : "b"(0xc0000000 | addr));

	return x;
}

static inline void
iwrite32(u32 addr, u32 x)
{
	asm volatile("stw %0,0(%1) ; eieio" : : "r"(x), "b"(0xc0000000 | addr));
}

static u32 _ipc_read(u32 reg) __attribute__((noinline));
static void _ipc_write(u32 reg, u32 value) __attribute__((noinline));
static void ipc_bell(u32 w) __attribute__((noinline));

// inline the 4*, don't inline the 0x0d0 stuff. yes, this saves a few bytes.
static u32
_ipc_read(u32 reg)
{
	return iread32(0x0d000000 + reg);
}

static void
_ipc_write(u32 reg, u32 value)
{
	iwrite32(0x0d000000 + reg, value);
}

static inline u32
ipc_read(u32 reg)
{
	return _ipc_read(4*reg);
}

static inline void
ipc_write(u32 reg, u32 value)
{
	_ipc_write(4*reg, value);
}

static void
ipc_bell(u32 w)
{
	ipc_write(1, w);
}

static void ios_delay(void) __attribute__((noinline));

static void
ios_delay(void)
{
	usleep(500);
}

static void
ipc_wait_ack(void)
{
	while(!(ipc_read(1) & 0x2))
		;
	ios_delay();
}

static void
ipc_wait_reply(void)
{
	while(!(ipc_read(1) & 0x4))
		;
	ios_delay();
}

static u32
ipc_wait(void)
{
	u32 ret;
	while(!((ret = ipc_read(1)) & 0x6))
		;
	ios_delay();
	return ret;
}

// Mid-level IPC access.

struct ipc {
	u32 cmd;
	int result;
	int fd;
	u32 arg[5];

	u32 user[8];
};

static struct ipc ipc ALIGNED(64);

static void
ipc_send_request(void)
{
	sync_after_write(&ipc, 0x40);

	ipc_write(0, (u32)virt_to_phys(&ipc));
	ipc_bell(1);

	ipc_wait_ack();

	ipc_bell(2);
}

static int
ipc_send_twoack(void)
{
	sync_after_write(&ipc, 0x40);
	ios_delay();
	
	ipc_write(0, (u32)virt_to_phys(&ipc));
	ipc_bell(1);

	if(ipc_wait() & 4)
		return 0;

	ipc_bell(2);

	if(ipc_wait() & 4)
		return 0;

	ipc_bell(2);
	ipc_bell(8);
	return 1;
}

static void
ipc_recv_reply(void)
{
	for (;;)
	{
		u32 reply;

		ipc_wait_reply();

		reply = ipc_read(2);
		ipc_bell(4);

		ipc_bell(8);

		if (((u32*)reply) == virt_to_phys(&ipc))
			break;
	}

	sync_before_read(&ipc, 0x40);
}


// High-level IPC access.

void
ios_cleanup()
{
	int loops = 0xA;
	do
	{
		if ((ipc_read(1) & 0x22) == 0x22)
		{
			ipc_write(1, (ipc_read(1)&~0x30) | 2);
		}
		if ((ipc_read(1) & 0x14) == 0x14)
		{
			ipc_read(2);
			ipc_write(1, (ipc_read(1)&~0x30) | 4);
			ipc_write(12, 0x4000);
			ipc_write(1, (ipc_read(1)&~0x30) | 8);
		}
		ipc_write(12, 0x4000);
		usleep(1000);
		loops--;
	} while(loops != 0);

	int fd;
	for (fd = 0; fd != 31; fd++)
	{
		ios_close(fd);
	}
}

int
ios_open(const char *filename, u32 mode)
{
	sync_after_write((void*)filename, 0x20);

	ipc.cmd = 1;
	ipc.fd = 0;
	ipc.arg[0] = (u32)virt_to_phys(filename);
	ipc.arg[1] = mode;

	ipc_send_request();
	ipc_recv_reply();

	return ipc.result;
}

int
ios_close(int fd)
{
	ipc.cmd = 2;
	ipc.fd = fd;

	ipc_send_request();
	ipc_recv_reply();

	return ipc.result;
}

static void
ios_std(int fd, int cmd)
{
	ipc.cmd = cmd;
	ipc.fd = fd;

	ipc_send_request();
	ipc_recv_reply();
}

int
ios_read(int fd, void *buf, u32 size)
{
	ipc.arg[0] = (u32)virt_to_phys(buf);
	ipc.arg[1] = size;

	ios_std(fd, 3);

	sync_before_read(buf, size);

	return ipc.result;
}

int
_ios_ioctlv(int fd, u32 n, u32 in_count, u32 out_count, struct ioctlv *vec, int reboot)
{
	u32 i;

	for (i = 0; i < in_count + out_count; i++)
	{
		if (vec[i].data)
		{
			sync_after_write(vec[i].data, vec[i].len);
			vec[i].data = (void *)virt_to_phys(vec[i].data);
		}
	}

	sync_after_write(vec, (in_count + out_count) * sizeof *vec);

	ipc.cmd = 7;
	ipc.fd = fd;
	ipc.arg[0] = n;
	ipc.arg[1] = in_count;
	ipc.arg[2] = out_count;
	ipc.arg[3] = (u32)virt_to_phys(vec);

	if(reboot)
	{
		if(ipc_send_twoack())
			return 0;
	}
	else
		ipc_send_request();
	ipc_recv_reply();

	for(i = in_count; i < in_count + out_count; i++)
	{
		if (vec[i].data)
		{
			vec[i].data = phys_to_virt((u32)vec[i].data);
			sync_before_read(vec[i].data, vec[i].len);
		}
	}
	if(reboot && (ipc.result >= 0))
		return -100;
	return ipc.result;
}

int
ios_ioctlv(int fd, u32 n, u32 in_count, u32 out_count, struct ioctlv *vec)
{
	return _ios_ioctlv(fd, n, in_count, out_count, vec, 0);
}

int
ios_ioctlvreboot(int fd, u32 n, u32 in_count, u32 out_count, struct ioctlv *vec)
{
	return _ios_ioctlv(fd, n, in_count, out_count, vec, 1);
}
