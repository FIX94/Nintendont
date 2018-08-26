#include <asm.h>

.set	AR_DMA_SAVE,		0x931C0040

.set	OSReport,			0xC0001860
.set	AR_DBG,				0x930031A0

#in
#	r29	u32 type
#	r28	u32 mainmem_addr
#	r30	u32 aram_addr
#	r31	u32 length

#out
#	none

ARStartDMA:
#	stwu	%sp,	-0x20(%sp)
#	stw		r7,	0xC(%sp)
#	stw		r8,	0x10(%sp)

#	lis		r7,	OSReport@h
#	ori		r7,	r7,	OSReport@l
#	stw		r28,	4(r7)
#	stw		r30,	8(r7)
#	stw		r31,	12(r7)

#	lis		r8,	AR_DBG@h
#	ori		r8,	r8,	AR_DBG@l
#	stw		r8,	0(r7)
#repeatprint:
#	lwz		r6,	0(r7)
#	cmpw	r6,	r8
#	beq		repeatprint

#	lwz		r7,	0xC(%sp)
#	lwz		r8,	0x10(%sp)
#	addi	%sp,	%sp,	0x20

	lis		r6,	AR_DMA_SAVE@h
	stwu	r29,	AR_DMA_SAVE@l(r6)
	stwu	r28,	4(r6)
	stwu	r30,	4(r6)
	stw		r31,	4(r6)

	lis		r6,	0xCC00
	blr
