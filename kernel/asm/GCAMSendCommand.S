#include <asm.h>

GCAMSendCommand:
	mflr	r0
	stw		r0,	4(%sp)
	stwu	%sp,	-8(%sp)

	lis		r4,	0				#gets replaced
	addi	r4,	r4,	0		#gets replaced
	addi	r6,	r4,	0x80

#flush DataIn
	clrrwi	r3,	r4,	5
	subf	r5,	r3,	r4
	addi	r5,	r5,	0x80	#static data length
	addi	r5,	r5,	0x1F
	srwi	r5,	r5,	5
	mtctr	r5

DCFlushRange:
	dcbf	0,	r3
	addi	r3,	r3,	0x20
	bdnz	DCFlushRange
	sync

	lis		r7,	0xD302
	lis		r0,	0x7000

	stw		r0,	0x6100(r7)
	stw		r4,	0x6104(r7)
	stw		r6,	0x6108(r7)

	li		r0,	3
	stw		r0,	0x6118(r7)

ready_loop:
	lwz		r0,	0x6118(r7)
	cmpwi	r0,	3
	beq		ready_loop

#invalidate DataOut
	clrrwi	r3,	r6,	5
	subf	r5,	r3,	r6
	addi	r5,	r5,	0x80	#static data length
	addi	r5,	r5,	0x1F
	srwi	r5,	r5,	5
	mtctr	r5

DCInvalidateRange:
	dcbi	0,	r3
	addi	r3,	r3,	0x20
	bdnz	DCInvalidateRange
	sync

	lis		r3,	0				#gets replaced
	addi	r3,	r3,	0		#gets replaced

	mtctr	r3
	li		r3,	0
	li		r4,	0
	bctrl

	li		r3,	1

	lwz		r0,	12(%sp)
	addi	%sp,	%sp,		8
	mtlr	r0
	blr
