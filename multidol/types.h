/*
	TinyLoad - a simple region free (original) game launcher in 4k

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

// Copyright 2008-2009  Hector Martin  <marcan@marcansoft.com>

#ifndef __TYPES_H__
#define __TYPES_H__

typedef volatile unsigned char		vu8;
typedef volatile unsigned short		vu16;
typedef volatile unsigned int		vu32;
typedef volatile unsigned long long	vu64;

typedef unsigned char		u8;
typedef unsigned short		u16;
typedef unsigned int		u32;
typedef unsigned long long	u64;

typedef char				s8;
typedef short				s16;
typedef int					s32;
typedef long long			s64;

#define NULL ((void *)0)

#define ALIGNED(n) __attribute__((aligned(n)))

#endif

