#include <asm.h>

.set	INT_BASE,	0xD3026500
.set	SI_INT,		INT_BASE+0x40

SIIntrruptHandler:
	#original instruction
	stw		%r0,	4(%sp)

	li		%r0,	0
	lis		%r8,	INT_BASE@h
	stw		%r0,	SI_INT@l(%r8)

	blr
