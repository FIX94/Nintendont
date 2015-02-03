#include <asm.h>

PADControlMotor:
	cmpwi	%r3,	0
	blt		exit
	cmpwi	%r3,	3
	bgt		exit

	lis		%r0,	0x9300
	ori		%r0,	%r0,	0x3010 #chan0 base
	slwi	%r3,	%r3,	2
	stwx	%r4,	%r3,	%r0

exit:
	blr
