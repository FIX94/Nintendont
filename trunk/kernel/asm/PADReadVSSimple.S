#include <asm.h>


PADReadVSSimple:

	cmplwi	%r4,	0xFF
	beq		FFEnd
	
	lis		%r3,	0xCD00

	cmplwi	%r4,	1
	beq		ReadPad2

	lwz		%r0,	0x6404(%r3)
	lwz		%r3,	0x6408(%r3)	

ReadPad:
	srwi	%r0,	%r0,	16

	li		%r3,	0x0

	cmplwi	%r5,	1
	beq		ReadOther

	andi.   %r5,	%r0,	0x800
	cmpwi	%r5,	0
	beq		YSkip
	ori		%r3,	%r3,	0x02
YSkip:

	andi.   %r5,	%r0,	0x1000
	cmpwi	%r5,	0
	beq		SSkip
	ori		%r3,	%r3,	0x80
SSkip:

	andi.   %r5,	%r0,	0x400
	cmpwi	%r5,	0
	beq		XSkip
	ori		%r3,	%r3,	0x01
XSkip:

	andi.   %r5,	%r0,	0x08
	cmpwi	%r5,	0
	beq		UpSkip
	ori		%r3,	%r3,	0x08
UpSkip:

	andi.   %r5,	%r0,	0x01
	cmpwi	%r5,	0
	beq		LeftSkip
	ori		%r3,	%r3,	0x04
LeftSkip:

	andi.   %r5,	%r0,	0x02
	cmpwi	%r5,	0
	beq		RightSkip
	ori		%r3,	%r3,	0x20
RightSkip:

end:
	blr
	
FFEnd:
	lis		%r3,	0
	b		end

ReadPad2:
	lwz		%r0,	0x6410(%r3)
	lwz		%r3,	0x6414(%r3)

	b		ReadPad

ReadOther:
	andi.   %r5,	%r0,	0x200
	cmpwi	%r5,	0
	beq		BSkip
	ori		%r3,	%r3,	0x40
BSkip:

	andi.   %r5,	%r0,	0x100
	cmpwi	%r5,	0
	beq		ASkip
	ori		%r3,	%r3,	0x80
ASkip:

	b		end
