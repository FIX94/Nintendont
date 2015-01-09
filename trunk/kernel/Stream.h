/*
Stream.c for Nintendont (Kernel)

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

#ifndef _STREAM_H_
#define _STREAM_H_

#include "global.h"

enum
{
	ONE_BLOCK_SIZE = 32,
	SAMPLES_PER_BLOCK = 28
};

void StreamInit();
void StreamStartStream(u32 CurrentStart, u32 CurrentSize);
void StreamEndStream();
void StreamUpdateRegisters();
void StreamPrepare();
void StreamUpdate();
u32 StreamGetChunkSize();

typedef void (*PCMWriter)();
void WritePCM48to32();
void WritePCM48();

#endif

