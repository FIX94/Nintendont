#include <asm.h>

.set DI_EXI_INT,	0xCD806828

__DVDInterruptHandler:
	lis		%r3,	0xCD80
	li		%r0,	0x34
	stw		%r0,	4(%r3)

	lwz		%r0,	DI_EXI_INT@l(%r3)
	rlwinm	%r0,	%r0,	0, 30, 28	#~4
	stw		%r0,	DI_EXI_INT@l(%r3)

	lis		%r0,	0x4000
	stw		%r0,	0x30(%r3)

	#original instruction
	lis		%r3,	0xD302
	blr
