/*
MajoraAudioStream.S for Nintendont (Kernel)

Copyright (C) 2015 FIX94

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

.set	audl,			10
.set	audr,			9
.set	oribuf,			5
.set	tmp,			28
.set	buf,			27
.set	loc,			26
.set	glob,			25
.set	cmpr,			6	#rarely touched
.set	cmpstrm,		5	#unused

MajoraAudioStream:
	#status saved in SaveRegs
	beq		cmpstrm,saveVars

check1:
	li		tmp,	0xC40
	slwi	tmp,	tmp,	4		#BUFSIZE=0xC400
	cmplw	cmpr,	loc,	tmp
	beq		cmpr,	updateStream
check2:
	slwi	tmp,	tmp,	1		#MAXBUF=0x18800
	cmplw	cmpr,	loc,	tmp
	beq		cmpr,	resetLoc
	b		checkInv
resetLoc:
	li		loc,	0

updateStream:
	li		tmp,	1
	stw		tmp,	UPDATE_STREAM@l(glob)

checkInv:
	lis		buf,	AI_ADP_BUF@h
	add		buf,	loc,	buf
	rlwinm	tmp,	buf,	0,27,31
	cmpwi	cmpr,	tmp,	0
	bne		cmpr,	insertL

	li		tmp,	0
DCInvalidateRange:
	dcbi	tmp,	buf

insertL:
#left audiostream
	lha		tmp,	2(buf)
#combine streams
	add		audl,	tmp,	audl
	cmpwi	cmpr,	audl,	0x4CCC
	bgt		cmpr,	compressPosL
	cmpwi	cmpr,	audl,	-0x4CCC
	blt		cmpr,	compressNegL
	b		insertR
compressPosL:
	subi	audl,	audl,	0x4CCC
	srawi	audl,	audl,	2
	addi	audl,	audl,	0x4CCC
	b		insertR
compressNegL:
	addi	audl,	audl,	0x4CCC
	srawi	audl,	audl,	2
	subi	audl,	audl,	0x4CCC

insertR:
#right audiostream
	lha		tmp,	0(buf)
#combine streams
	add		audr,	tmp,	audr
	cmpwi	cmpr,	audr,	0x4CCC
	bgt		cmpr,	compressPosR
	cmpwi	cmpr,	audr,	-0x4CCC
	blt		cmpr,	compressNegR
	b		exit
compressPosR:
	subi	audr,	audr,	0x4CCC
	srawi	audr,	audr,	2
	addi	audr,	audr,	0x4CCC
	b		exit
compressNegR:
	addi	audr,	audr,	0x4CCC
	srawi	audr,	audr,	2
	subi	audr,	audr,	0x4CCC

exit:
	addi	loc,	loc,	4

saveVars:
	sth		audl,	2(oribuf)
	sth		audr,	0(oribuf)
	nop		#back to loop
