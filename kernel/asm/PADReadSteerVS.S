#include <asm.h>

.set		PadBuff,	0x93002800

PADReadSteerVS:
	lis		%r3,	PadBuff@h

	cmpwi	%r4,	0
	beq		CheckStickY

	cmpwi	%r4,	1
	beq		CheckStickX

	cmpwi	%r4,	2
	beq		CheckStickY_P2

	cmpwi	%r4,	3
	beq		CheckStickX_P2

NoUpdate:
	li		%r3,	0
	b		end

CheckStickY:
	lbz		%r3,	0x2803(%r3)
	addi	%r3,	%r3,	0x80
	not		%r3,	%r3
	rlwinm	%r3,	%r3,	7,		17,		23
	b		end

CheckStickX:
	lbz		%r3,	0x2802(%r3)
	addi	%r3,	%r3,	0x80
	rlwinm	%r3,	%r3,	7,		17,		23
	b		end

CheckStickY_P2:
	lbz		%r3,	0x280F(%r3)
	addi	%r3,	%r3,	0x80
	not		%r3,	%r3
	rlwinm	%r3,	%r3,	7,		17,		23
	b		end

CheckStickX_P2:
	lbz		%r3,	0x280E(%r3)
	addi	%r3,	%r3,	0x80
	rlwinm	%r3,	%r3,	7,		17,		23

end:
	blr
