#include <asm.h>

TCIntrruptHandler:

	stw		%r0, 4(%sp)
	
	lis		%r8,	0xCD00
	li		%r0,	0x34
	stw		%r0,	4(%r8)

	lis		%r8,	0xD201
	li		%r0,	0
	stw		%r0,	0(%r8)

	lis		%r8,	0xCD00
	lis		%r0,	0x4000
	stw		%r0,	0x30(%r8)

	blr
