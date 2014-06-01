#include <asm.h>


PADReadB:
	and     %r4,	%r0,	%r4
	stw     %r4,	8(%r3)
	stw     %r0,	0xC(%r3)

	lis		%r4,	0xCD00
	lwz		%r0,	0x6404(%r4)
	lwz		%r4,	0x6408(%r4)

	srwi	%r0,	%r0,	16

	li		%r4,	0x0

	andi.   %r5,	%r0,	0x100
	cmpwi	%r5,	0
	beq		ASkip
	ori		%r4,	%r4,	0x6000
ASkip:

	andi.   %r5,	%r0,	0x200
	cmpwi	%r5,	0
	beq		BSkip
	ori		%r4,	%r4,	0x400
BSkip:

#	andi.   %r5,	%r0,	0x010
#	cmpwi	%r5,	0
#	beq		ZSkip
#	ori		%r4,	%r4,	0x1000
#ZSkip:
#
	andi.   %r5,	%r0,	0x1000
	cmpwi	%r5,	0
	beq		SSkip
	ori		%r4,	%r4,	0x400
SSkip:

	lwz		%r0,	0x8(%r3)
	or		%r4,	%r4,	%r0
	stw		%r4,	0x8(%r3)

	addi	%r1,	%r1,	0x20

	blr
