#include <asm.h>

.set		NinCfg,			0xD3002900
.set		NinCfgConfig,	NinCfg + 0x8

#in
#	r3	channel

#out
#	r3 res 

EXIProbe:
	cmpwi	%r3,	0
	bne		False
	lis		%r3,	NinCfgConfig@h
	lwz		%r3,	NinCfgConfig@l(%r3)
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
