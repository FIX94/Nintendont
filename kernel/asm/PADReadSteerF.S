#include <asm.h>

.set		PadBuff,	0x93002800

PADReadSteerF:
	lis		%r5,	PadBuff@h

	cmpwi	%r4,	0
	beq		CheckStickX

	cmpwi	%r4,	1
	beq		CheckStickY

	cmpwi	%r4,	4
	beq		CheckA

	cmpwi	%r4,	5
	beq		CheckB

NoUpdate:
	li		%r3,	0
	blr

CheckStickX:
	lbz		%r3,	0x2802(%r5)
	addi	%r3,	%r3,	0x80
	slwi	%r3,	%r3,	8
	blr

CheckStickY:
	lbz		%r3,	0x2803(%r5)
	addi	%r3,	%r3,	0x80
	slwi	%r3,	%r3,	8
	blr

CheckA:
	lhz		%r3,	PadBuff@l(%r5)
	andi.	%r3,	%r3,	0x100
	beqlr
	li		%r3,	0x7F00
	blr

CheckB:
	lhz		%r3,	PadBuff@l(%r5)
	andi.	%r3,	%r3,	0x200
	beqlr
	li		%r3,	0x7F00
	blr
