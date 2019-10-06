#include <asm.h>
.include "constants.inc"

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
