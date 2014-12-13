#include <asm.h>

GCAMSendCommand:
	mflr	%r0
	stw		%r0,	4(%sp)
	stwu	%sp,	-8(%sp)

#flush memory for READ
	lis		%r4,	0				#gets replaced
	addi	%r4,	%r4,	0		#gets replaced
	addi	%r6,	%r4,	0x80

	li		%r0,	0
	lis		%r7,	0xC000
	addi	%r7,	%r7,	0x2F60
#IPC area
	dcbi	%r0,	%r7

#flush DataIn
	clrrwi	%r3,	%r4,	5
	li		%r4,	0x80
	li		%r0,	0

	clrlwi	%r5,	%r3,	27
	add		%r4,	%r4,	%r5
	addi	%r4,	%r4,	0x1F
	srwi	%r4,	%r4,	5
	mtctr	%r4
flushA:
	dcbf	%r0,	%r3
	addi	%r3,	%r3,	0x20
	bdnz	flushA

	li		%r5,	0x80
#flush DataOut
	clrrwi	%r3,	%r6,	5
	li		%r4,	0x80
	li		%r0,	0

	clrlwi	%r5,	%r3,	27
	add		%r4,	%r4,	%r5
	addi	%r4,	%r4,	0x1F
	srwi	%r4,	%r4,	5
	mtctr	%r4
flushB:
	dcbi	%r0,	%r3
	addi	%r3,	%r3,	0x20
	bdnz	flushB

	lis		%r7,	0xC000
	lis		%r0,	0x7000

	stw		%r0,	0x2F60(%r7)

	subi	%r3,	%r6,		0x80
	stw		%r3,	0x2F64(%r7)
	stw		%r6,	0x2F68(%r7)

	li		%r0,	3
	stw		%r0,	0x2F78(%r7)

ready_loop:
	lwz		%r0,	0x2F78(%r7)
	cmpwi	%r0,	3
	beq		ready_loop

wait_loop:
	lwz		%r0,	0x2F9C(%r7)
	andi.	%r0,	%r0,	0x14
	cmpwi	%r0,	0
	beq		wait_loop

	lis		%r3,	0				#gets replaced
	addi	%r3,	%r3,	0		#gets replaced

	mtctr	%r3
	li		%r3,	0
	li		%r4,	0
	bctrl

	li		%r3,		1

	lwz		%r0,	12(%sp)
	addi	%sp,	%sp,		8
	mtlr	%r0
	blr
