#include <asm.h>

.set		PadStub,	0x93000000
.set		PadStubHID,	0x93001000
.set		PadBuff,	0x93002800

PADRead:
	mflr	%r4
	#to stub instead of arm
	lis		%r6,			PadStub@h
	ori		%r6,	%r6,	PadStub@l
	mtctr	%r6
	bctrl
	#to hid stub
	lis		%r6,			PadStubHID@h
	ori		%r6,	%r6,	PadStubHID@l
	mtctr	%r6
	bctrl

	#Assumes r4 is unmodified by PadStub
	mtlr	%r4
	lis		%r5,			PadBuff@h
	ori		%r5,	%r5,	PadBuff@l

	#original code
	li		%r4,	0x30

memcpy:
	lwz		%r6,		0(%r5)
	lwz		%r7,		4(%r5)
	lwz		%r8,		8(%r5)
	addi	%r5,		%r5,		0xC
	stw		%r6,		0(%r3)
	stw		%r7,		4(%r3)
	stw		%r8,		8(%r3)
	addi	%r3,		%r3,		0xC
	subi	%r4,		%r4,		0xC
	cmpwi	%r4,		0
	bne		memcpy

	blr
