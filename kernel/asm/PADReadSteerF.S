#include <asm.h>

.set		PadBuff,	0x93002800

PADReadSteerF:
	lis		%r3,	PadBuff@h

	cmpwi	%r4,	0
	beq		CheckStickX

	cmpwi	%r4,	1
	beq		CheckStickY

	cmpwi	%r4,	4
	beq		CheckA

	cmpwi	%r4,	5
	beq		CheckB

NoUpdate:
	li		%r3,	0
	b		end

CheckStickX:
	lbz		%r3,	0x2802(%r3)
	addi	%r3,	%r3,	0x80
	slwi	%r3,	%r3,	8
	b		end

CheckStickY:
	lbz		%r3,	0x2803(%r3)
	addi	%r3,	%r3,	0x80
	slwi	%r3,	%r3,	8
	b		end

CheckA:
	lhz		%r3,	PadBuff@l(%r3)
	andi.	%r3,	%r3,	0x100
	beq		end
	li		%r3,	0x7F00
	b		end

CheckB:
	lhz		%r3,	PadBuff@l(%r3)
	andi.	%r3,	%r3,	0x200
	beq		end
	li		%r3,	0x7F00

end:
	blr
