#include <asm.h>

.set		PadStub,	0x93000000
.set		PadStubHID,	0x93001000
.set		PadBuff,	0x93002800

PADReadB:
	mflr	%r9
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

	#Assumes r9 is unmodified by PadStub
	mtlr	%r9
	lis		%r5,			PadBuff@h
	ori		%r5,	%r5,	PadBuff@l
	dcbst	0,		%r5 ; sync ; isync

	#original code
	and     %r4,	%r0,	%r4
	stw     %r4,	8(%r3)
	stw     %r0,	0xC(%r3)

	lwz		%r0,	0(%r5)
	srwi	%r0,	%r0,	16

	li		%r4,	0x0

	andi.   %r5,	%r0,	0x100
	cmpwi	%r5,	0
	beq		ASkip
	ori		%r4,	%r4,	0x6000
ASkip:

	andi.   %r5,	%r0,	0x200
	cmpwi	%r5,	0
	beq		BSkip
	ori		%r4,	%r4,	0x400
BSkip:

#	andi.   %r5,	%r0,	0x010
#	cmpwi	%r5,	0
#	beq		ZSkip
#	ori		%r4,	%r4,	0x1000
#ZSkip:
#
	andi.   %r5,	%r0,	0x1000
	cmpwi	%r5,	0
	beq		SSkip
	ori		%r4,	%r4,	0x400
SSkip:

	lwz		%r0,	0x8(%r3)
	or		%r0,	%r4,	%r0
	stw		%r0,	0x8(%r3)

	lwz		%r0,	0xC(%r3)
	or		%r0,	%r4,	%r0
	stw		%r0,	0xC(%r3)

	addi	%r1,	%r1,	0x20

	blr
