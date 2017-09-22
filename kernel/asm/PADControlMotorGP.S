#include <asm.h>
.include "constants.inc"

PADControlMotorGP:
	#original instruction
	lbz		%r4,	0x2F(%r31)
	#load control motor base into r3
	lis		%r3,	PadRumble@h
	ori		%r3,	%r3,	PadRumble@l
	#start by checking if rumble was active
	lbz		%r5,	0(%r3)
	cmpwi	%r5,	0
	beq		checkRumble
	#was active, extend rumble
	subi	%r5,	%r5,	1
	stb		%r5,	0(%r3)
	b		rumbleOn
checkRumble:
	#was not active so check now
	li		%r0,	0
	cmpwi	%r4,	0
	beq		saveRumble
	#did not rumble before, set up
	li		%r5,	15
	stb		%r5,	0(%r3)
rumbleOn:
	li		%r0,	1
saveRumble:
	stb		%r0,	3(%r3)
	blr
