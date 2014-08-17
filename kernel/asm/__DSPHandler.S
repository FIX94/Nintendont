/*
__DSPHandler.S for Nintendont (Kernel)

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

.set	AI_VR,			0xCD006C04
.set	AI_ADP_BUF,		0x93280000

.set	STREAM_BASE,	0xD3026520
.set	REAL_STREAMING,	0xD302652C
.set	AI_MEM_BASE,	0xD3026560
.set	AI_MEM_SIZE,	0xD3026564

.set	AI_ADP_LOC,		0xD3026580

#note:r3 and r12 will get overwritten, safe to use

__DSPHandler:
	lis		%r12,	STREAM_BASE@h
	lwz		%r3,	REAL_STREAMING@l(%r12)
	cmpwi	%r3,	0
	bne		startcp
	blr

startcp:
	stwu	%r1,	-0x20(%r1)
	stw		%r4,	0x0(%r1)
	stw		%r5,	0x4(%r1)
	stw		%r6,	0x8(%r1)
	stw		%r7,	0xC(%r1)
	stw		%r8,	0x10(%r1)
	stw		%r9,	0x14(%r1)
	stw		%r0,	0x18(%r1)
	mflr	%r0
	stw		%r0,	0x1C(%r1)

	lwz		%r9,	AI_MEM_BASE@l(%r12)
	lwz		%r8,	AI_MEM_SIZE@l(%r12)

	lwz		%r4,	AI_ADP_LOC@l(%r12)
	li		%r3,	0x1500
	slwi	%r3,	%r3,	4		#BUFSIZE=0x15000
	cmplw	%r4,	%r3
	blt		buf1
buf2:
	slwi	%r3,	%r3,	1		#MAXBUF=0x2A000
buf1:
	lis		%r6,	AI_ADP_BUF@h

	srwi	%r7,	%r8,	5
	mtctr	%r7
	mr		%r5,	%r6
	add		%r5,	%r5,	%r4
	li		%r0,	0
DCInvalidateRange1:
	dcbi	%r0,	%r5
	addi	%r5,	%r5,	0x20
	bdnz	DCInvalidateRange1

	srwi	%r7,	%r8,	5
	mtctr	%r7
	mr		%r5,	%r9
	li		%r0,	0
DCInvalidateRange2:
	dcbi	%r0,	%r5
	addi	%r5,	%r5,	0x20
	bdnz	DCInvalidateRange2

	li		%r5,	0
startcopy:
#left audiostream
	lhax	%r0,	%r6,	%r4
#calc volume
	lis		%r7,	AI_VR@h
	lwz		%r7,	AI_VR@l(%r7)
	clrlwi	%r7,	%r7,	24
	mullw	%r0,	%r7,	%r0
	srawi	%r0,	%r0,	8
#left original
	lhax	%r7,	%r9,	%r5
#combine streams
	bl		combinebuffer
#save new stream
	sthx	%r7,	%r9,	%r5

	addi	%r4,	%r4,	2
	addi	%r5,	%r5,	2

#right audiostream
	lhax	%r0,	%r6,	%r4
#calc volume
	lis		%r7,	AI_VR@h
	lwz		%r7,	AI_VR@l(%r7)
	extrwi	%r7,	%r7,	8,		16
	mullw	%r0,	%r7,	%r0
	srawi	%r0,	%r0,	8
#right original
	lhax	%r7,	%r9,	%r5
#combine streams
	bl		combinebuffer
#save new stream
	sthx	%r7,	%r9,	%r5

	addi	%r4,	%r4,	2
	addi	%r5,	%r5,	2

	cmplw	%r4,	%r3
	beq		changebuffer
copy2:
	cmplw	%r5,	%r8
	blt		startcopy

	srwi	%r7,	%r8,	5
	mtctr	%r7
	mr		%r5,	%r9
	li		%r0,	0
DCFlushRange:
	dcbf	%r0,	%r5
	addi	%r5,	%r5,	0x20
	bdnz	DCFlushRange

exit:
	stw		%r4,	AI_ADP_LOC@l(%r12)

	lwz		%r0,	0x1C(%r1)
	mtlr	%r0
	lwz		%r4,	0x0(%r1)
	lwz		%r5,	0x4(%r1)
	lwz		%r6,	0x8(%r1)
	lwz		%r7,	0xC(%r1)
	lwz		%r8,	0x10(%r1)
	lwz		%r9,	0x14(%r1)
	lwz		%r0,	0x18(%r1)
	cmpwi	%r0,	0
	addi	%r1,	%r1,	0x20
	blr

changebuffer:
	lis		%r7,	STREAM_BASE@h
	li		%r0,	1
	stw		%r0,	STREAM_BASE@l(%r7)
	li		%r0,	0x1500
	slwi	%r0,	%r0,	4		#BUFSIZE=0x15000
	cmplw	%r4,	%r0
	beq		copy2
	li		%r4,	0
	b		copy2

#the most simple linear compressor I could come up with
combinebuffer:
	add		%r7,	%r7,	%r0
	cmpwi	%r7,	0x4CCC
	bgt		compressPos
	cmpwi	%r7,	-0x4CCC
	blt		compressNeg
	blr
compressPos:
	subi	%r7,	%r7,	0x4CCC
	srawi	%r7,	%r7,	2
	addi	%r7,	%r7,	0x4CCC
	blr
compressNeg:
	addi	%r7,	%r7,	0x4CCC
	srawi	%r7,	%r7,	2
	subi	%r7,	%r7,	0x4CCC
	blr
