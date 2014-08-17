/*
DVDLowAudioStream.S for Nintendont (Kernel)

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

.set	STREAM_BASE,	0xD3026520
.set	STREAM_UPDATE,	0xD3026524
.set	REAL_STREAMING,	0xD302652C
.set	FAKE_STREAMING,	0xD3026540
.set	STREAM_LENGTH,	0xD3026544
.set	STREAM_START,	0xD3026548
.set	REALSTREAM_END,	0xD302654C
.set	LOOP_ENABLED,	0xD3026584

DVDLowAudioStream:
	stwu	%r1,	-0x20(%r1)
	mflr	%r0
	stw		%r0,	0(%r1)
	mtctr	%r6

	lis		%r6,	1
	cmpw	%r3,	%r6
	lis		%r6,	STREAM_BASE@h
	bne		setupstream
	li		%r3,	0
	stw		%r3,	REAL_STREAMING@l(%r6)
	stw		%r3,	FAKE_STREAMING@l(%r6)
	stw		%r3,	STREAM_LENGTH@l(%r6)
	stw		%r3,	STREAM_START@l(%r6)
	stw		%r3,	REALSTREAM_END@l(%r6)
	b		startcb

setupstream:
	cmpwi	%r4,	0
	bne		startstream
	li		%r3,	0
	stw		%r3,	LOOP_ENABLED@l(%r6)
	b		startcb

startstream:
	stw		%r4,	STREAM_LENGTH@l(%r6)
	stw		%r5,	STREAM_START@l(%r6)
	li		%r3,	1
	stw		%r3,	LOOP_ENABLED@l(%r6)
	stw		%r3,	FAKE_STREAMING@l(%r6)
	stw		%r3,	STREAM_UPDATE@l(%r6)

startcb:
	li		%r3,	1		#for the cb that everything resulted ok
	bctrl
	lwz		%r0,	0(%r1)
	mtlr	%r0
	addi	%r1,	%r1,	0x20
	blr
