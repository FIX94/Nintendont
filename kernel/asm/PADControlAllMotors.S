#include <asm.h>
.include "constants.inc"

PADControlAllMotors:
	lis		%r4,	PadRumble@h
	ori		%r4,	%r4,	PadRumble@l	#chan0 base

	li		%r5,	3

repeat:
	slwi	%r6,	%r5,	2
	lwzx	%r0,	%r6,	%r3
	stwx	%r0,	%r6,	%r4
	cmpwi	%r5,	0
	beq		exit
	subi	%r5,	%r5,	1
	b		repeat

exit:
	blr
