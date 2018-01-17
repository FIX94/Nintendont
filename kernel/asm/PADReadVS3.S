#include <asm.h>
.include "constants.inc"

PADReadVS3:
	#original instruction
	mr		%r27,	%r3

	stwu	%sp,	-0x20(%sp)
	mflr	%r0
	stw		%r0,	0xC(%sp)

	#to stub instead of arm
	lis		%r3,			PadStub@h
	ori		%r3,	%r3,	PadStub@l
	mtctr	%r3
	bctrl

	lis		%r3,			PadBuff@h
	ori		%r3,	%r3,	PadBuff@l
	li		%r0,	0
	#store player data for ARM JVSIO.c
	dcbf	%r0,	%r3

	lwz		%r0,	0xC(%sp)
	mtlr	%r0
	addi	%sp,	%sp,	0x20

	blr
