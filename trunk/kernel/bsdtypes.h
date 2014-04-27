/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	BSD types compatibility layer for the SD host controller driver.

Copyright (C) 2008, 2009	Sven Peter <svenpeter@gmail.com>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __BSDTYPES_H__
#define __BSDTYPES_H__

#include "global.h"
#include "errno.h"

typedef u32 u_int;
//typedef u32 u_int32_t;
typedef u16 u_int16_t;
typedef u8 u_int8_t;
typedef u8 u_char;

typedef u32 bus_space_tag_t;
typedef u32 bus_space_handle_t;

struct device {
	char dv_xname[255];
	void *dummy;
};

#define MIN(a, b) (((a)>(b))?(b):(a))

#define wakeup(...)

#define bzero(mem, size) memset8(mem, 0, size)

#define ISSET(var, mask) (((var) & (mask)) ? 1 : 0)
#define SET(var, mask) ((var) |= (mask))

#endif


