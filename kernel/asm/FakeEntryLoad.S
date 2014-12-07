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

	li		%r0,	0
	lwz		%r4,	DIP_CMD_1@l(%r3)
	mtctr	%r4
	stw		%r0,	DIP_CMD_1@l(%r3)

	lwz		%r4,	DIP_CMD_2@l(%r3)
	stw		%r0,	DIP_CMD_2@l(%r3)

InvalidateRange1:
	dcbi	%r0,	%r4
	icbi	%r0,	%r4
	addi	%r4,	%r4,	0x20
	bdnz	InvalidateRange1

	li		%r4,	0xC0
	mtctr	%r4
	lis		%r4,	0x8000
	ori		%r4,	%r4,	0x1800

InvalidateRange2:
	dcbi	%r0,	%r4
	icbi	%r0,	%r4
	addi	%r4,	%r4,	0x20
	bdnz	InvalidateRange2

	sync
	isync
	blr
