#include <asm.h>

#

__OSInitSRAM:
	li		%r5,	64
	mr		%r3,	%r30
	
memcpy_pre:	
	lwz		%r0,	0(%r4)
	stw		%r0,	0(%r3)
	addic.	%r5,	%r5,	-4

memcpy:
	lwzu	%r0,	4(%r4)
	stwu	%r0,	4(%r3)
	addic.	%r5,	%r5,	-4
	bne		memcpy
	
	li		%r31,	1
	stw		%r31,	0x4C(%r30)

	blr
