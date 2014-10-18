#include <asm.h>

.set		PadStub,	0x93000000
.set		PadBuff,	0x93002800

PADReadB:
	#hack already stored -0x20, save values for later
	stmw	%r27,	0xC(%sp)
	mr		%r27,	%r3
	mr		%r28,	%r4
	mr		%r29,	%r0
	mflr	%r30

	#to stub instead of arm
	lis		%r4,			PadStub@h
	ori		%r4,	%r4,	PadStub@l
	mtctr	%r4
	bctrl


	li		%r3,	0
	lis		%r4,	PadBuff@h
	lhz		%r0,	PadBuff@l(%r4)

CheckA:
	andi.	%r4,	%r0,	0x100
	beq		CheckB
	ori		%r3,	%r3,	0x6000

CheckB:
	andi.	%r4,	%r0,	0x200
	beq		CheckZ
	ori		%r3,	%r3,	0x400

CheckZ:
#	andi.	%r4,	%r0,	0x010
#	beq		CheckStart
#	ori		%r3,	%r3,	0x1000

CheckStart:
	andi.	%r4,	%r0,	0x1000
	beq		ButtonUpdate
	ori		%r3,	%r3,	0x400

ButtonUpdate:
	mr		%r4,	%r28
	mr		%r0,	%r29

	or		%r4,	%r4,	%r3
	or		%r0,	%r0,	%r3

	mr		%r3,	%r27

#original two stores
	stw		%r4,	8(%r3)
	stw		%r0,	0xC(%r3)

	mtlr	%r30
	lmw		%r27,	0xC(%sp)
	addi	%sp,	%sp,	0x20
	blr
