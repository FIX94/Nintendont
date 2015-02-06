#include <asm.h>

.set		PadStub,	0x93000000
.set		PadBuff,	0x93002800

PADReadGP:
	cmpwi	%r0,	0
	beq		PADUpdate

	cmpwi	%r0,	1
	beq		CheckRightTrigger

	cmpwi	%r0,	2
	beq		CheckLeftTrigger

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

CheckStickX:
	lis		%r3,	PadBuff@h
	lbz		%r3,	0x2802(%r3)
	addi	%r3,	%r3,	0x80
	slwi	%r3,	%r3,	8
	b		end
#################################
CheckRightTrigger:
	lis		%r3,	PadBuff@h
	lbz		%r3,	0x2807(%r3)
	rlwinm	%r3,	%r3,	7,		17,		23 #reduce to 0x7F
	b		end
#################################
CheckLeftTrigger:
	lis		%r3,	PadBuff@h
	lbz		%r3,	0x2806(%r3)
	rlwinm	%r3,	%r3,	7,		17,		23 #reduce to 0x7F
#################################
end:
	blr
