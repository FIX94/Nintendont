/*
	TinyLoad - a simple region free (original) game launcher in 4k

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

// Copyright 2008-2009  Hector Martin  <marcan@marcansoft.com>

#ifndef __UTILS_H__
#define __UTILS_H__

#include "types.h"

void usleep(u32 us);
void _memcpy(void *ptr, const void *src, u32 size);

#endif
