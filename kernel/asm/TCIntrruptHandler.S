#include <asm.h>

.set	INT_BASE,	0xD3026500
.set	EXI_INT,	INT_BASE+0x60

TCIntrruptHandler:
	#original instruction
	stw		%r0,	4(%sp)

	li		%r0,	0
	lis		%r8,	INT_BASE@h
	stw		%r0,	EXI_INT@l(%r8)

	blr
