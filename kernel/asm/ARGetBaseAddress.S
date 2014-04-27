#include <asm.h>

ARGetBaseAddress:
	lis		%r3,	0x2000
	oris	%r3,	%r3,	0x4000
	blr
