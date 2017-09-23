#include <asm.h>
.include "constants.inc"

PADIsBarrel:
	cmpwi	%r3,	0
	blt		invalid
	cmpwi	%r3,	3
	bgt		invalid

	lis		%r0,	0xD300
	ori		%r0,	%r0,	0x3130 #chan0 base
	slwi	%r3,	%r3,	2
	lwzx	%r3,	%r3,	%r0
	blr

invalid:
	li		%r3,	0
	blr
