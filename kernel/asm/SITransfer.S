#include <asm.h>

# issue read command
#
#	r3	channel
#	r4	inData
#	r5	inLen
#	r6	outData
#	r7	outLen
#	r8	cb
#	r9	0?
#	r10	timeout?


SITransfer:

	mflr    r0
	stw     r0, 4(%sp)
	stwu    %sp, -0x60(%sp)
	stmw    r19, 0x2C(%sp)
		
	mr		r19,	r3
	mr		r10,	r4
	bl		OSDisableInterrupts
	
	mr		r21,	r3
	mr		r4,	r10

	lis     r9,	0xD302
	lis     r0,	0x5000
	or		r0,	r0,	r19
	
	stw     r0,	0x6100(r9)
	
	stw     r4,	0x6104(r9)
	stw     r5,	0x6108(r9)
	stw     r6,	0x610C(r9)
	stw     r7,	0x6110(r9)

#flush DataIn
	clrrwi	r3,	r4,	5
	subf	r4,	r3,	r4
	add		r4,	r4,	r5
	addi	r4,	r4,	0x1F
	srwi	r4,	r4,	5
	mtctr	r4

DCFlushRange:
	dcbf	0,	r3
	addi	r3,	r3,	0x20
	bdnz	DCFlushRange
	sync

	li		r0,	3
	stw		r0,	0x6118(r9)

ready_loop:
	lwz		r0,	0x6118(r9)
	cmpwi	r0,	3
	beq		ready_loop

#invalidate DataOut
	clrrwi	r3,	r6,	5
	subf	r4,	r3,	r6
	add		r4,	r4,	r7
	addi	r4,	r4,	0x1F
	srwi	r4,	r4,	5
	mtctr	r4

DCInvalidateRange:
	dcbi	0,	r3
	addi	r3,	r3,	0x20
	bdnz	DCInvalidateRange
	sync

	cmpwi	r8,	0
	beq		skip_cb
	mtctr	r8
	li      r3,	0
	li      r4,	0
	bctrl

skip_cb:

	mr		r3,	r21
	bl		OSRestoreInterrupts
	
	li      r3,	1

	lmw     r19, 0x2C(%sp)
	lwz     r0,	0x64(%sp)
	addi    %sp, %sp, 0x60
	mtlr    r0
	blr

OSDisableInterrupts:
	mfmsr   r3
	rlwinm  r4, r3, 0,17,15
	mtmsr   r4
	extrwi  r3, r3, 1,16
	blr

OSRestoreInterrupts:
	cmpwi   r3, 0
	mfmsr   r4
	beq     loc_8001EA58
	ori     r5, r4, 0x8000
	b       loc_8001EA5C
loc_8001EA58:
	rlwinm  r5, r4, 0,17,15
loc_8001EA5C:
	mtmsr   r5
	extrwi  r3, r4, 1,16
	blr
