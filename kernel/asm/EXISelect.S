#include <asm.h>

#in
#	r3	channel
#	r4	device
#	r5	freq

#out
#	r3	res

EXISelect:
	lis		%r7,	0xCD80
	lis		%r0,	0x1000

	slwi	%r4,	%r4,	8
	slwi	%r5,	%r5,	16

	or		%r0,	%r0,	%r3
	or		%r0,	%r0,	%r4
	or		%r0,	%r0,	%r5

	stw		%r0,	0x600C(%r7)

ready_loop:
	lwz		%r3,	0x600C(%r7)
	cmpw	%r3,	%r0
	beq		ready_loop

	lwz		%r3,	0x6010(%r7)
	blr
