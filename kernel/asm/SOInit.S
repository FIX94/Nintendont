#include <asm.h>

.set SO_QUEUE,	0x930269C0
.set __OSSetInterruptHandler,0x930269B8
.set __OSUnmaskInterrupts,0x930269BC

SOInit:
	stwu	%sp,	-0x20(%sp)
	mflr	%r0
	stw		%r0,	0xC(%sp)

	li %r3, 5
	mtctr %r3
	lis %r3, SO_QUEUE@h
	ori %r3, %r3, SO_QUEUE@l
	li %r0, 0
initThreadQueue:
	stw %r0, 0(%r3)
	stw %r0, 4(%r3)
	addi %r3, %r3, 8
	bdnz initThreadQueue

	li %r3,0x1A
	lis %r4,0 #HSPIntrruptHandler@h
	ori %r4,%r4,0 #HSPIntrruptHandler@l

	lis %r5,__OSSetInterruptHandler@h
	lwz %r5,__OSSetInterruptHandler@l(%r5)
	mtlr %r5
	blrl

	li %r3,0x20
	lis %r4,__OSUnmaskInterrupts@h
	lwz %r4,__OSUnmaskInterrupts@l(%r4)
	mtlr %r4
	blrl

	lwz		%r0,	0xC(%sp)
	addi	%sp,	%sp,	0x20
	mtlr	%r0
	blr
