#include <asm.h>

PADControlMotor:
	lis		%r5,	0xD300
	ori		%r5,	%r5,	0x3010 #chan0 base
findchan:
	cmpwi	%r3,	0
	beq		copy
	subi	%r3,	%r3,	1
	addi	%r5,	%r5,	4
	b findchan
copy:
	stw		%r4,	0(%r5)
	blr
