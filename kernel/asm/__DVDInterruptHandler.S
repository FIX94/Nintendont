/*
__DVDInterruptHandler.S for Nintendont (Kernel)

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

.set	DIP_STATUS,		0xCD806000
.set	DI_STATUS,		0xD3026000

.set	DI_SDMA_ADR,	0xD3026044
.set	DI_SDMA_LEN,	0xD3026048
.set	DI_SCONTROL,	0xD302604C

__DVDInterruptHandler:
	#write back own status to clear its interrupts
	lis		%r3,	DIP_STATUS@h
	lwz		%r4,	DIP_STATUS@l(%r3)
	stw		%r4,	DIP_STATUS@l(%r3)

	#save original dvd status to current status
	lis		%r3,	DI_STATUS@h
	stw		%r4,	DI_STATUS@l(%r3)

	#check if we need to flush data
	lwz		%r4,	DI_SCONTROL@l(%r3)
	cmpwi	%r4,	3	#DMA Read
	bne		NoFlush

	#load original address from shadow
	lwz		%r4,	DI_SDMA_ADR@l(%r3)
	lwz		%r0,	DI_SDMA_LEN@l(%r3)

	srwi	%r0,	%r0,	5
	cmpwi	%r0,	0
	bne		NoZero
	li		%r0,	1
NoZero:
	mtctr	%r0

	li		%r0,	0
InvalidateRange:
	dcbi	%r0,	%r4
	icbi	%r0,	%r4
	addi	%r4,	%r4,	0x20
	bdnz	InvalidateRange

NoFlush:
	blr
