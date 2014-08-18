/*
DVDLowRequestAudioStatus.S for Nintendont (Kernel)

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
#include <asm.h>

.set	DI_SIMM,		0xC0002F50

.set	STREAM_BASE,	0xD3026520
.set	STREAM_UPDATE,	0xD3026524
.set	FAKE_STREAMING,	0xD3026540
.set	STREAM_LENGTH,	0xD3026544
.set	STREAM_START,	0xD3026548
.set	STREAM_CURRENT,	0xD302654C
.set	REALSTREAM_END,	0xD3026550

DVDLowRequestAudioStatus:
	mtctr	%r4
	lis		%r4,	STREAM_BASE@h
	srwi	%r3,	%r3,	16		#cmd is in upper 16-bit
	cmpwi	%cr5,	%r3,	0
	beq		%cr5,	Streaming
	cmpwi	%cr5,	%r3,	1
	beq		%cr5,	CurrentOffset
	cmpwi	%cr5,	%r3,	2
	beq		%cr5,	StartOffset

StartLength:
	lwz		%r3,	STREAM_LENGTH@l(%r4)
	b		SetDI_SIMM

Streaming:
	lwz		%r3,	REALSTREAM_END@l(%r4)
	cmpwi	%cr5,	%r3,	0
	beq		%cr5,	FakeStreaming
#stream ended, did a new one begin already
	lwz		%r3,	STREAM_UPDATE@l(%r4)
	cmpwi	%cr5,	%r3,	1
	beq		%cr5,	FakeStreaming
#real stream ended, no update
	li		%r3,	0
	b		SetDI_SIMM
FakeStreaming:
	lwz		%r3,	FAKE_STREAMING@l(%r4)
	b		SetDI_SIMM

CurrentOffset:
	lwz		%r3,	STREAM_CURRENT@l(%r4)
	b		ShiftOffset

StartOffset:
	lwz		%r3,	STREAM_START@l(%r4)
ShiftOffset:
	srwi	%r3,	%r3,	2

SetDI_SIMM:
	lis		%r4,	DI_SIMM@h
	stw		%r3,	DI_SIMM@l(%r4)
	li		%r3,	1		#for the cb that everything resulted ok
	bctr		#will boot cb then return to original function, saves quite some code
