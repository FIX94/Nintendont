#include <asm.h>

.set		PadStub,	0x93000000
.set		PadBuff,	0x93002800

PADReadVS:
	cmplwi	%r4,	0
	beq		P1ButtonUpdate

	cmplwi	%r4,	1
	beq		P2ButtonUpdate

NoUpdate:
	li		%r3,	0
	b		end
#################################
P1ButtonUpdate:
	cmplwi	%r5,	0
	beq		PADUpdate

	lis		%r4,	PadBuff@h
	lhz		%r0,	PadBuff@l(%r4)
	b		UpdateAB
#################################
P2ButtonUpdate:
	cmplwi	%r5,	0
	beq		PAD2Update

	lis		%r4,	PadBuff@h
	lhz		%r0,	0x280C(%r4)
	b		UpdateAB
#################################
PADUpdate:
	stwu	%sp,	-0x20(%sp)
	mflr	%r0
	stw		%r0,	0xC(%sp)

	#to stub instead of arm
	lis		%r3,			PadStub@h
	ori		%r3,	%r3,	PadStub@l
	mtctr	%r3
	bctrl

	lwz		%r0,	0xC(%sp)
	mtlr	%r0
	addi	%sp,	%sp,	0x20

	lis		%r4,	PadBuff@h
	lhz		%r0,	PadBuff@l(%r4)
	b		UpdateMain
#################################
PAD2Update:
	lis		%r4,	PadBuff@h
	lhz		%r0,	0x280C(%r4)
#################################
UpdateMain:
	li		%r3,	0

CheckX:
	andi.	%r4,	%r0,	0x400
	beq		CheckY
	ori		%r3,	%r3,	0x01

CheckY:
	andi.	%r4,	%r0,	0x800
	beq		CheckLeft
	ori		%r3,	%r3,	0x02

CheckLeft:
	andi.	%r4,	%r0,	0x01
	beq		CheckUp
	ori		%r3,	%r3,	0x04

CheckUp:
	andi.	%r4,	%r0,	0x08
	beq		CheckRight
	ori		%r3,	%r3,	0x08

CheckRight:
	andi.	%r4,	%r0,	0x02
	beq		CheckStart
	ori		%r3,	%r3,	0x20

CheckStart:
	andi.	%r4,	%r0,	0x1000
	beq		end
	ori		%r3,	%r3,	0x80
	b		end
#################################
UpdateAB:
	li		%r3,	0

CheckA:
	andi.	%r4,	%r0,	0x100
	beq		CheckB
	ori		%r3,	%r3,	0x80

CheckB:
	andi.	%r4,	%r0,	0x200
	beq		end
	ori		%r3,	%r3,	0x40
#################################
end:
	blr
