#include <asm.h>

.set		PadStub,	0x93000000
.set		PadBuff,	0x93002800

PADReadF:
	stwu	%sp,	-0x20(%sp)
	mflr	%r0
	stw		%r0,	0x24(%sp)
	stmw	%r27,	0xC(%sp)
	mr		%r28,	%r4

	#to stub instead of arm
	lis		%r6,			PadStub@h
	ori		%r6,	%r6,	PadStub@l
	mtctr	%r6
	bctrl

	mr		%r4,	%r28
	lmw		%r27,	0xC(%sp)
	lwz		%r0,	0x24(%sp)
	mtlr	%r0
	addi	%sp,	%sp,	0x20

	li		%r3,	0
	lis		%r6,	PadBuff@h

	cmplwi	%r4,	1
	beq		CheckLeftTrigger

	cmplwi	%r4,	0xFF
	beq		end

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
	rlwinm	%r5,	%r0,	4,		24,		27
	or		%r3,	%r3,	%r5
	b		end

CheckLeftTrigger:
	lbz		%r5,	0x2806(%r6)
	cmplwi	%r5,	0x44
	ble		CheckRightTrigger
	ori		%r3,	%r3,	0x20

CheckRightTrigger:
	lbz		%r5,	0x2807(%r6)
	cmplwi	%r5,	0x44
	ble		end
	ori		%r3,	%r3,	0x10
end:
	blr
