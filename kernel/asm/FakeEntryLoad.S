#include <asm.h>

.set	DIP_CMD_1,		0xCD80600C
.set	DIP_CMD_2,		0xCD806010
.set	DIP_IMM,		0xCD806020

FakeEntryLoad:
	lis		%r3,	DIP_IMM@h
	li		%r0,	0x4DEA
	stw		%r0,	DIP_IMM@l(%r3)
wait_loop:
	lwz		%r4,	DIP_IMM@l(%r3)
	cmplw	%r4,	%r0
	beq		wait_loop
	mtlr	%r4

	lwz		%r4,	DIP_CMD_1@l(%r3)
	mtctr	%r4

	lwz		%r3,	DIP_CMD_2@l(%r3)
	li		%r0,	0
InvalidateRange:
	dcbi	%r0,	%r3
	icbi	%r0,	%r3
	addi	%r3,	%r3,	0x20
	bdnz	InvalidateRange
	blr
