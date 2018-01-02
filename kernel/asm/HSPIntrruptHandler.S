#include <asm.h>

.set OSWakeupThread,0x930269B4

.set SO_QUEUE,	0x930269C0

.set	INT_BASE,	0xD3026500
.set	EXI_INT,	INT_BASE+0x60
.set	EXI_INT_FD,	INT_BASE+0x64

HSPIntrruptHandler:
	stwu	%sp,	-0x20(%sp)
	mflr	%r0
	stw		%r0,	0xC(%sp)
	stw		%r31,	0x10(%sp)

	lis		%r3,	INT_BASE@h
	lwz		%r4,	EXI_INT_FD@l(%r3)
	slwi	%r4,	%r4,	3
	lis		%r3,	SO_QUEUE@h
	ori		%r3,	%r3,	SO_QUEUE@l
	add		%r3,	%r3,	%r4
	lis		%r4,	OSWakeupThread@h
	lwz		%r4,	OSWakeupThread@l(%r4)
	mtlr	%r4
	blrl

	li		%r0,	0
	lis		%r3,	INT_BASE@h
	stw		%r0,	EXI_INT@l(%r3)

	lwz		%r0,	0xC(%sp)
	lwz		%r31,	0x10(%sp)
	addi	%sp,	%sp,	0x20
	mtlr	%r0
	blr
