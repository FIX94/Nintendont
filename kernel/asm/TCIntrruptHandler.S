#include <asm.h>

.set DI_EXI_INT,	0xCD806828

TCIntrruptHandler:
	#original instruction
	stw		%r0,	4(%sp)

	lis		%r8,	0xCD80
	li		%r0,	0x34
	stw		%r0,	4(%r8)

	lwz		%r0,	DI_EXI_INT@l(%r8)
	rlwinm	%r0,	%r0,	0, 28, 26	#~0x10
	stw		%r0,	DI_EXI_INT@l(%r8)

	lis		%r0,	0x4000
	stw		%r0,	0x30(%r8)

	blr
