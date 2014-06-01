#include <asm.h>


PADReadSteer:

	lis		%r3,	0xCD00

	cmpwi	%r0,	0
	bne		SteerSkip
	lwz		%r3,	0x6404(%r3)
	andi.	%r3,	%r3,	0xFF00
	b		end
SteerSkip:

	cmpwi	%r0,	1
	bne		AccSkip
	lwz		%r3,	0x6408(%r3)
	rlwinm	%r3,	%r3, 8, 16, 23
	cmpwi	%r3,	0x2200
	ble		SZ
	subi	%r3,	%r3,	0x22
	srwi	%r3,	%r3,	1
	b		AccSkip
SZ:
	li		%r3,	0
AccSkip:

	cmpwi	%r0,	2
	bne		end
	lwz		%r3,	0x6408(%r3)
	andi.	%r3,	%r3,	0xFF00
	cmpwi	%r3,	0x2200
	ble		AZ
	subi	%r3,	%r3,	0x2200
	b		end
AZ:
	li		%r3,	0

end:
	blr
