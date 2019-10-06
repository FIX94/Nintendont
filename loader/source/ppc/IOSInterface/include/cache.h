/*
	TinyLoad - a simple region free (original) game launcher in 4k

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

/* This code comes from the Twilight Hack */
// Copyright 2008-2009  Segher Boessenkool  <segher@kernel.crashing.org>
// Copyright 2008-2009  Hector Martin  <marcan@marcansoft.com>

#ifndef __CACHE_H__
#define __CACHE_H__

typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

void sync_before_read(void *p, u32 len);
void sync_after_write(const void *p, u32 len);
void sync_after_store(const void *p, u32 len);
void sync_before_exec(const void *p, u32 len);

#endif
