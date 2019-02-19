#include <asm.h>

.set	PRS_DOL,		0xD31C0000
.set	PRS_EXTRACT,	0xD31C0020

SwitcherPrs:
	stwu	%sp,	-0x20(%sp)
	mflr	r0
	stw		r0,	8(%sp)
	stw		r31,	0x10(%sp)

	lis		r31,	PRS_EXTRACT@h
	lwz		r0,	PRS_EXTRACT@l(r31)	#offset saved from Patch.c
	mtlr	r0

	blrl	#extracts switcher

	lis		r31,	PRS_DOL@h
	lwz		r0,	PRS_DOL@l(r31)	#FakeEntryLoad
	stw		r0,	0xE0(r28)	#modify dol entry

	lwz		r31,	0x10(%sp)
	lwz		r0,	8(%sp)
	mtlr	r0
	addi	%sp,	%sp,	0x20

	blr
