#include <asm.h>

#in
#	r3	channel
#	r4	dev
#	r5	ID buffer

#out
#	r3 res 

EXIGetID:
	cmpwi	%r3,	0
	beq		True
	li		%r3,	0
	blr
True:
	cmpwi	%r4,	0
	beq		TrueB
	li		%r3,	0
	blr
TrueB:
	li		%r3,	0x80
	stw		%r3,	0x0(%r5)
	
	lis		%r4,	0x8000
	lwz		%r3,	0x30C0(%r4)

	blr
