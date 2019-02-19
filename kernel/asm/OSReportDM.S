#include <asm.h>

.set	OSReport,			0xC0001860

OSReportDM:
	stwu	%sp,	-0x20(%sp)
	mflr	r0
	stw		r0,	0xC(%sp)
	stw		r9,	0x10(%sp)
	lis		r9,	OSReport@h

	stw		r4,	0x1864(r9)
	stw		r5,	0x1868(r9)
	stw		r6,	0x186C(r9)
	stw		r7,	0x1870(r9)
	stw		r8,	0x1874(r9)

	stw		r3,	0x1860(r9)
repeatcheck:
	lwz		r0,	0x1860(r9)
	cmpw	r0,	r3
	beq		repeatcheck

	lwz		r0,	0xC(%sp)
	lwz		r9,	0x10(%sp)
	addi	%sp,	%sp,	0x20
	mtlr	r0
	blr
