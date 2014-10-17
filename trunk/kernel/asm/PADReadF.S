#include <asm.h>

.set		PadStub,	0x93000000
.set		PadBuff,	0x93002800

PADReadF:
	cmplwi	%r4,	0
	beq		PADUpdate

	cmplwi	%r4,	1
	beq		TriggerUpdate

NoUpdate:
	li		%r3,	0
	blr

PADUpdate:
	stwu	%sp,	-0x20(%sp)
	mflr	%r0
	stw		%r0,	0x24(%sp)
	stmw	%r27,	0xC(%sp)

	#to stub instead of arm
	lis		%r6,			PadStub@h
	ori		%r6,	%r6,	PadStub@l
	mtctr	%r6
	bctrl

	lmw		%r27,	0xC(%sp)
	lwz		%r0,	0x24(%sp)
	mtlr	%r0
	addi	%sp,	%sp,	0x20

	li		%r3,	0
	lis		%r6,	PadBuff@h
	lhz		%r0,	PadBuff@l(%r6)

CheckY:
	andi.	%r5,	%r0,	0x800
	beq		CheckStart
	ori		%r3,	%r3,	0x02

CheckStart:
	andi.	%r5,	%r0,	0x1000
	beq		CheckDPAD
	ori		%r3,	%r3,	0x80

CheckDPAD:
	rlwimi	%r3,	%r0,	4,		26,		27	#left,right
	rlwimi	%r3,	%r0,	0,		28,		29	#down,up
	blr

TriggerUpdate:
	li		%r3,	0
	lis		%r6,	PadBuff@h
CheckLeftTrigger:
	lbz		%r5,	0x2806(%r6)
	cmplwi	%r5,	0x44
	ble		CheckRightTrigger
	ori		%r3,	%r3,	0x20

CheckRightTrigger:
	lbz		%r5,	0x2807(%r6)
	cmplwi	%r5,	0x44
	blelr
	ori		%r3,	%r3,	0x10
	blr
