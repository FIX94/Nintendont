#include <asm.h>

.set		PadBuff,	0x93002800

PADReadSteerF:
	li		%r3,	0
	lis		%r5,	PadBuff@h

	cmpwi	%r4,	0
	bne		CheckStickY

CheckStickX:
	lbz		%r3,	0x2802(%r5)
	addi	%r3,	%r3,	0x80
	slwi	%r3,	%r3,	8
	b		end

CheckStickY:
	cmpwi	%r4,	1
	bne		CheckA

	lbz		%r3,	0x2803(%r5)
	addi	%r3,	%r3,	0x80
	slwi	%r3,	%r3,	8
	b		end

CheckA:
	cmpwi	%r4,	4
	bne		CheckB

	lhz		%r3,	PadBuff@l(%r5)
	andi.	%r3,	%r3,	0x100
	beq		end
	li		%r3,	0x7F00
	b		end

CheckB:
	cmpwi	%r4,	5
	bne		end

	lhz		%r3,	PadBuff@l(%r5)
	andi.	%r3,	%r3,	0x200
	beq		end
	li		%r3,	0x7F00

end:
	blr
