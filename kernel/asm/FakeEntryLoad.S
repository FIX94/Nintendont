#include <asm.h>

.set	RESET_STATUS,	0xD3003420
.set	FLUSH_LEN,		0xD3003424
.set	FLUSH_ADDR,		0xD3003428

FakeEntryLoad:
	lis		%r3,	RESET_STATUS@h
	li		%r0,	0x4DEA
	stw		%r0,	RESET_STATUS@l(%r3)
wait_loop:
	lwz		%r4,	RESET_STATUS@l(%r3)
	cmplw	%r4,	%r0
	beq		wait_loop
	mtlr	%r4

	lwz		%r4,	FLUSH_LEN@l(%r3)
	mtctr	%r4

	li		%r0,	0
	lwz		%r4,	FLUSH_ADDR@l(%r3)
InvalidateRange1:
	dcbi	%r0,	%r4
	icbi	%r0,	%r4
	addi	%r4,	%r4,	0x20
	bdnz	InvalidateRange1

	li		%r4,	0x100
	mtctr	%r4
	lis		%r4,	0x8000
	ori		%r4,	%r4,	0x1000

InvalidateRange2:
	dcbi	%r0,	%r4
	icbi	%r0,	%r4
	addi	%r4,	%r4,	0x20
	bdnz	InvalidateRange2

	sync
	isync

	blr
