/*
	TinyLoad - a simple region free (original) game launcher in 4k

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

/* This code comes from HBC's stub which was based on geckoloader and the Twilight Hack code */
/* Some of these routines are from public domain sources */
// Copyright 2008-2009  Segher Boessenkool  <segher@kernel.crashing.org>
// Copyright 2008-2009  Andre Heider  <dhewg@wiibrew.org>
// Copyright 2008-2009  Hector Martin  <marcan@marcansoft.com>

#include "types.h"
#include "utils.h"

static u32 get_time(void)
{
	u32 x;

	asm volatile("mftb %0" : "=r"(x));

	return x;
}

void usleep(u32 us)
{
	u32 _start = get_time();
	while ((get_time() - _start) < (91*us)) ;
}

void _memcpy(void *ptr, const void *src, u32 size)
{
	char* ptr2 = ptr;
	const char* src2 = src;
	while(size--) *ptr2++ = *src2++;
}

void _memset(void *ptr, int c, u32 size)
{
	char* ptr2 = ptr;
	while(size--) *ptr2++ = c;
}