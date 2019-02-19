#include <asm.h>
.include "constants.inc"

PADReadF:
	#only update player 1
	cmplwi	r28,	0
	bne		end

	stwu	%sp,	-0x20(%sp)
	mflr	r0
	stw		r0,	0xC(%sp)

	#to stub instead of arm
	lis		r3,		PadStub@h
	ori		r3,	r3,	PadStub@l
	mtctr	r3
	bctrl

	lis		r3,		PadBuff@h
	ori		r3,	r3,	PadBuff@l
	#store player data for ARM JVSIO.c
	dcbf	0,	r3

	lwz		r0,	0xC(%sp)
	mtlr	r0
	addi	%sp,	%sp,	0x20

end:
	#original instruction
	mr		r3,	r31
	blr
