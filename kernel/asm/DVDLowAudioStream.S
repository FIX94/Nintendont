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
.include "constants.inc"

DVDLowAudioStream:
	mtctr	%r6
	lis		%r6,	1
	cmpw	%r3,	%r6
	lis		%r6,	STREAM_BASE@h
	bne		SetupStream

StopStream:
	li		%r3,	0
	sth		%r3,	REAL_STREAMING@l(%r6)
	stw		%r3,	FAKE_STREAMING@l(%r6)
	stw		%r3,	STREAM_LENGTH@l(%r6)
	stw		%r3,	STREAM_START@l(%r6)
	stw		%r3,	STREAM_CURRENT@l(%r6)
	stw		%r3,	REALSTREAM_END@l(%r6)
	b		StartCB

SetupStream:
	cmpwi	%r4,	0
	bne		StartStream
	li		%r3,	0
	stw		%r3,	LOOP_ENABLED@l(%r6)
	b		StartCB

StartStream:
	stw		%r4,	STREAM_LENGTH@l(%r6)
	stw		%r5,	STREAM_START@l(%r6)
	stw		%r5,	STREAM_CURRENT@l(%r6)
	li		%r3,	1
	stw		%r3,	LOOP_ENABLED@l(%r6)
	stw		%r3,	FAKE_STREAMING@l(%r6)
	stw		%r3,	STREAM_UPDATE@l(%r6)

StartCB:
	li		%r3,	1		#for the cb that everything resulted ok
	bctr		#will boot cb then return to original function, saves quite some code
