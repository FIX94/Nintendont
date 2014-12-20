#include <asm.h>

.set	DI_SCMD_1,		0xD302603C
.set	DI_SCMD_2,		0xD3026040
.set	DI_SIMM,		0xD3026050

FakeEntryLoad:
	lis		%r3,	DI_SIMM@h
	li		%r0,	0x4DEA
	stw		%r0,	DI_SIMM@l(%r3)
wait_loop:
	lwz		%r4,	DI_SIMM@l(%r3)
	cmplw	%r4,	%r0
	beq		wait_loop
	mtlr	%r4

	li		%r0,	0
	lwz		%r4,	DI_SCMD_1@l(%r3)
	mtctr	%r4
	stw		%r0,	DI_SCMD_1@l(%r3)

	lwz		%r4,	DI_SCMD_2@l(%r3)
	stw		%r0,	DI_SCMD_2@l(%r3)

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
