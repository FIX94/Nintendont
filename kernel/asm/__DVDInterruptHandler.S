#include <asm.h>

.set	INT_BASE,	0xD3026500
.set	DI_INT,		INT_BASE+0x20

__DVDInterruptHandler:
	li		%r0,	0
	lis		%r3,	INT_BASE@h
	stw		%r0,	DI_INT@l(%r3)

	#original instruction
	lis		%r3,	0xD302

	blr
