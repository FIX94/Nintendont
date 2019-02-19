#include <asm.h>

.set	SI_GC_CONTROLLER,		0x09000000
.set	SI_ERROR_NO_RESPONSE,	0x08
.set	PAD_USED_BASE,			0x93003020

#
#
#	r3	channel

SIGetType:
	lis		r4,		PAD_USED_BASE@h
	ori		r4,	r4,	PAD_USED_BASE@l

DCInvalidateRange:
	dcbi	0,	r4
	sync

	lwz		r0,	4(r4)
	li		r4,	1
	slw		r4,	r4,	r3
	and		r4,	r0,	r4
	cmpwi	r4,	0
	beq		NoPad

	lis		r3,	SI_GC_CONTROLLER@h
	blr

NoPad:
	li		r3,	SI_ERROR_NO_RESPONSE@l
	blr
