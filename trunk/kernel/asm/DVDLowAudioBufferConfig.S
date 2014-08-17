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

.set	REAL_STREAMING,	0xD302652C

DVDLowAudioBufferConfig:
	mtctr	%r5
	lis		%r5,	REAL_STREAMING@h
	cmpwi	%r3,	0
	beq		StopStream
StartStream:
	li		%r3,	1
	b		SaveValue
StopStream:
	li		%r3,	0
SaveValue:
	stw		%r3,	REAL_STREAMING@l(%r5)
cb:
	li		%r3,	1
	bctrl
	blr
