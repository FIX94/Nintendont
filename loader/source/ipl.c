#include <gccore.h>
#include <string.h>
#include "ipl.h"

// bootrom descrambler reversed by segher
// Copyright 2008 Segher Boessenkool <segher@kernel.crashing.org>
void Descrambler(unsigned char* data, unsigned int size)
{
	unsigned char acc = 0;
	unsigned char nacc = 0;

	unsigned short t = 0x2953;
	unsigned short u = 0xd9c2;
	unsigned short v = 0x3ff1;

	unsigned char x = 1;
	unsigned int it;
	for (it = 0; it < size; )
	{
		int t0 = t & 1;
		int t1 = (t >> 1) & 1;
		int u0 = u & 1;
		int u1 = (u >> 1) & 1;
		int v0 = v & 1;

		x ^= t1 ^ v0;
		x ^= (u0 | u1);
		x ^= (t0 ^ u1 ^ v0) & (t0 ^ u0);

		if (t0 == u0)
		{
			v >>= 1;
			if (v0)
				v ^= 0xb3d0;
		}

		if (t0 == 0)
		{
			u >>= 1;
			if (u0)
				u ^= 0xfb10;
		}

		t >>= 1;
		if (t0)
			t ^= 0xa740;

		nacc++;
		acc = 2*acc + x;
		if (nacc == 8)
		{
			data[it++] ^= acc;
			nacc = 0;
		}
	}
}

#define IPL_ROM_FONT_SJIS	0x1AFF00

#define DECRYPT_START		0x100
#define CODE_START			0x820

void load_ipl(unsigned char *buf)
{
	Descrambler(buf + DECRYPT_START, IPL_ROM_FONT_SJIS - DECRYPT_START);
	memcpy((void*)0x81300000, buf + CODE_START, IPL_ROM_FONT_SJIS - CODE_START);
	DCFlushRange((void*)0x81300000, IPL_ROM_FONT_SJIS - CODE_START);
}
