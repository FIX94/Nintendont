#include <asm.h>

#in
#	r3	channel

#out
#	r3 res 

EXIProbe:
	cmpwi	%r3,	0
	bne		False
#   NIN_CFG.Config = 0x13002A18 + 0x8
	lis		%r3,	0xD300
	lwz		%r3,	0x2A20(%r3)
	andi.	%r3,	%r3,	0x8
	cmpwi	%r3,	0
	bne		True
False:
	li		%r3,	0
	blr
True:
	lis		%r3,	0xE2D3
	ori		%r3,	%r3,	0x83C1

	lis		%r4,	0x8000
	stw		%r3,	0x30C0(%r4)

	li		%r3,	1
	blr
