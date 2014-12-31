#include <asm.h>

.set		PadBuff,	0x93002800

PADReadSteer:
	lis		%r3,			PadBuff@h

	cmpwi	%r0,	0
	beq		CheckStickX

	cmpwi	%r0,	1
	beq		CheckRightTrigger

	cmpwi	%r0,	2
	beq		CheckLeftTrigger

NoUpdate:
	li		%r3,	0
	b		end

CheckStickX:
	lbz		%r3,	0x2802(%r3)
	addi	%r3,	%r3,	0x80
	slwi	%r3,	%r3,	8
	b		end

CheckRightTrigger:
	lbz		%r3,	0x2807(%r3)
	rlwinm	%r3,	%r3,	7,		17,		23 #reduce to 0x7F
	b		end

CheckLeftTrigger:
	lbz		%r3,	0x2806(%r3)
	rlwinm	%r3,	%r3,	7,		17,		23 #reduce to 0x7F

end:
	blr
