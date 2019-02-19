#include <asm.h>
.include "constants.inc"

TCIntrruptHandler:
	#original instruction
	stw		r0,	4(%sp)

	li		r0,	0
	lis		r8,	INT_BASE@h
	stw		r0,	EXI_INT@l(r8)

	blr
