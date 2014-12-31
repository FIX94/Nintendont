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
	b		end
#################################
PADUpdate:
	stwu	%sp,	-0x20(%sp)
	mflr	%r0
	stw		%r0,	0xC(%sp)

	#to stub instead of arm
	lis		%r3,			PadStub@h
	ori		%r3,	%r3,	PadStub@l
	mtctr	%r3
	bctrl

	lwz		%r0,	0xC(%sp)
	mtlr	%r0
	addi	%sp,	%sp,	0x20

	li		%r3,	0
	lis		%r4,	PadBuff@h
	lhz		%r0,	PadBuff@l(%r4)

CheckY:
	andi.	%r4,	%r0,	0x800
	beq		CheckStart
	ori		%r3,	%r3,	0x02

CheckStart:
	andi.	%r4,	%r0,	0x1000
	beq		CheckDPAD
	ori		%r3,	%r3,	0x80

CheckDPAD:
	rlwimi	%r3,	%r0,	4,		26,		27	#left,right
	rlwimi	%r3,	%r0,	0,		28,		29	#down,up
	b		end
#################################
TriggerUpdate:
	li		%r3,	0
	lis		%r4,	PadBuff@h

CheckLeftTrigger:
	lbz		%r0,	0x2806(%r4)
	cmplwi	%r0,	0x44
	ble		CheckRightTrigger
	ori		%r3,	%r3,	0x20

CheckRightTrigger:
	lbz		%r0,	0x2807(%r4)
	cmplwi	%r0,	0x44
	ble		end
	ori		%r3,	%r3,	0x10
#################################
end:
	blr
