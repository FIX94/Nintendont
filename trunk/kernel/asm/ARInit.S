#include <asm.h>

ARInit:
	oris	%r3,	%r3,	0x2000
	blr

	lis		%r0,	0x2000
	ori		%r0,	%r0,	0x4000
	blr
