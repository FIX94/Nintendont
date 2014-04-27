/*
	TinyLoad - a simple region free (original) game launcher in 4k

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

/* This code comes from the Twilight Hack */
// Copyright 2008-2009  Segher Boessenkool  <segher@kernel.crashing.org>
// Copyright 2008-2009  Hector Martin  <marcan@marcansoft.com>

#include "types.h"

void sync_before_read(void *p, u32 len)
{
	u32 a, b;

	a = (u32)p & ~0x1f;
	b = ((u32)p + len + 0x1f) & ~0x1f;

	for ( ; a < b; a += 32)
		asm("dcbi 0,%0" : : "b"(a) : "memory");

	asm("sync ; isync");
}

void sync_after_write(const void *p, u32 len)
{
	u32 a, b;

	a = (u32)p & ~0x1f;
	b = ((u32)p + len + 0x1f) & ~0x1f;

	for ( ; a < b; a += 32)
		asm("dcbf 0,%0" : : "b"(a));

	asm("sync ; isync");
}

