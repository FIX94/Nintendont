#include <asm.h>

.set		PadBuff,	0x93002800

#
#
#	r3	channel

SIGetType:
	lis		%r4,			PadBuff@h
	ori		%r4,	%r4,	PadBuff@l
	mulli 	%r3,	%r3,	12
	add		%r4, 	%r4,	%r3
	lwz 	%r5,	8(%r4)

	cmpwi	%r5,	0
	bne		NoPad
	lis		%r3,	0x0900
	b		end

NoPad:
	li		%r3,	0x80
	b		end

end:
	blr
