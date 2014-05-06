#include <asm.h>

DCInvalidateRange:
	lis		%r3,	0xC000
	lwz		%r4,	0x2F40(%r3)
	lwz		%r3,	0x2F44(%r3)

	srwi	%r4,	%r4,	5
	cmpwi	%r4,	0
	bne		NoZero
	li		%r4,	1
NoZero:
	mtctr	%r4

InvalidateRange:
	dcbi	%r0,	%r3
	icbi	%r0,	%r3
	addi	%r3,	%r3,	0x20
	bdnz	InvalidateRange

	lis		%r3,	0xCD80
	li		%r0,	0

	blr
