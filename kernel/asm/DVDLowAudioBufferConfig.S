/*
DVDLowAudioBufferConfig.S for Nintendont (Kernel)

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

DVDLowAudioBufferConfig:
	mtctr	%r5
	lis		%r5,	REAL_STREAMING@h
	cmpwi	%r3,	0
	beq		StopStream

StartStream:
	lhz		%r3,	REAL_STREAMING@l(%r5)
	cmpwi	%r3,	0x40
	bne		StartCB		#dont start when not paused before

	li		%r3,	0x20
	b		SaveValue

StopStream:
	lhz		%r3,	REAL_STREAMING@l(%r5)
	cmpwi	%r3,	0x20
	bne		StartCB		#dont pause when not even streaming

	li		%r3,	0x40
SaveValue:
	sth		%r3,	REAL_STREAMING@l(%r5)
StartCB:
	li		%r3,	1		#for the cb that everything resulted ok
	bctr		#will boot cb then return to original function, saves quite some code
