#include <asm.h>

.set	DI_BASE,	0xD3026000
.set	DI_INV_ADR,	(DI_BASE+0x28)
.set	DI_INV_LEN,	(DI_BASE+0x2C)

.set	INT_BASE,	0xD3026500
.set	DI_INT,		(INT_BASE+0x20)

__DVDInterruptHandler:
	li		r0,	0
	lis		r3,	INT_BASE@h
	stw		r0,	DI_INT@l(r3)

	lwz		r0,	DI_INV_LEN@l(r3)
	cmplwi	r0,	0
	beq		end

InvalidatePrep:
	srwi	r0,	r0,	5
	mtctr	r0
	li		r0,	0
	stw		r0,	DI_INV_LEN@l(r3)

	lwz		r3,	DI_INV_ADR@l(r3)
InvalidateRange:
	dcbi	0,	r3
	icbi	0,	r3
	addi	r3,	r3,	0x20
	bdnz	InvalidateRange

end:
	#original instructions
	nop
	lis		r3,	DI_BASE@h

	blr
