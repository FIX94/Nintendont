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
	lwz		%r5,	DIP_STATUS@l(%r3)
	stw		%r5,	DIP_STATUS@l(%r3)

	#save original dvd status to current status
	lis		%r3,	DI_STATUS@h
	stw		%r5,	DI_STATUS@l(%r3)

	#check if we need to flush data
	lwz		%r5,	DI_SCONTROL@l(%r3)
	cmpwi	%r5,	3	#DMA Read
	bne		NoFlush

	#load original address from shadow
	lwz		%r5,	DI_SDMA_ADR@l(%r3)
	lwz		%r6,	DI_SDMA_LEN@l(%r3)

	clrrwi	%r3,	%r5,	5
	subf	%r5,	%r3,	%r5
	add		%r6,	%r5,	%r6
	addi	%r6,	%r6,	0x1F
	srwi	%r6,	%r6,	5
	mtctr	%r6

	li		%r0,	0
InvalidateRange:
	dcbi	%r0,	%r3
	icbi	%r0,	%r3
	addi	%r3,	%r3,	0x20
	bdnz	InvalidateRange

NoFlush:
	# Replaced in Datel version with lis %rX, DIP_STATUS@h
	lis		%r3,	DI_STATUS@h
	blr
