#include <asm.h>

PADControlAllMotors:
	lis		%r4,	0xD300
	ori		%r4,	%r4,	0x3010 #chan0 base

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
