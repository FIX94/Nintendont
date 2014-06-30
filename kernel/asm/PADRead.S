#include <asm.h>

.set		PadStub,	0x93000000
.set		PadBuff,	0x93002800

PADRead:
	mflr	%r0
	stw		%r0,	4(%sp)
	stwu	%sp,	-0x40(%sp)
	stw		%r31,	0x18(%sp)
	mr		%r31,	%r3


	#to stub instead of arm
	lis		%r6,			PadStub@h
	ori		%r6,	%r6,	PadStub@l
	mtctr	%r6
	bctrl

	lis		%r5,			PadBuff@h
	ori		%r5,	%r5,	PadBuff@l

	#original code
	li		%r4,	0x30

memcpy:
	lwz		%r6,		0(%r5)
	lwz		%r7,		4(%r5)
	lwz		%r8,		8(%r5)
	addi	%r5,		%r5,		0xC
	stw		%r6,		0(%r31)
	stw		%r7,		4(%r31)
	stw		%r8,		8(%r31)
	addi	%r31,		%r31,		0xC
	subi	%r4,		%r4,		0xC
	cmpwi	%r4,		0
	bne		memcpy

	lwz		%r31,	0x18(%sp)
	lwz		%r0,	0x44(%sp)
	addi	%sp,	%sp,	0x40
	mtlr	%r0
	blr
