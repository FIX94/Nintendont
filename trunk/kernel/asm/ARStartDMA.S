#include <asm.h>

.set	AR_DMA_CNT,	0xCC005028
.set	AR_STR,		0x930028A0
#in
#	r3	u32 type
#	r4	u32 mainmem_addr
#	r5	u32 aram_addr
#	r6	u32 length

#out
#	none

ARStartDMA:
	stwu	%sp,	-0x10(%sp)
	mflr	%r0
	stw		%r0,	8(%sp)

	cmpwi	%r6,	0
	beq		end

	rlwinm	%r4,	%r4,	0,		7,		26 # 0x01ffffe0
	oris	%r4,	%r4,	0x8000

	rlwinm	%r5,	%r5,	0,		8,		26 # 0x00ffffe0
	oris	%r5,	%r5,	0x9000

#	lis		%r7,	AR_STR@h
#	ori		%r0,	%r7,	AR_STR@l
#	lis		%r7,	0xC000
#	stw		%r0,	0x1860(%r7)
#	stw		%r4,	0x1864(%r7)
#	stw		%r5,	0x1868(%r7)
#	stw		%r6,	0x186C(%r7)
#wait_loop:
#	lwz		%r0,	0x1860(%r7)
#	cmpw	%r0,	%r3
#	beq		wait_loop

	cmpwi	%r3,	1
	bne		MRAM_TO_ARAM

ARAM_TO_MRAM:
	mr		%r7,	%r5
	mr		%r5,	%r4
	mr		%r4,	%r7

MRAM_TO_ARAM:
	srwi	%r7,	%r6,	5
	mtctr	%r7
	mr		%r7,	%r4

	li		%r0,	0
DCInvalidateRange:
	dcbi	%r0,	%r7
	addi	%r7,	%r7,	0x20
	bdnz	DCInvalidateRange

memcpy_pre:
	mr		%r7,	%r6		#save length
	lwz		%r0,	0(%r4)
	stw		%r0,	0(%r5)
	addic.	%r7,	%r7,	-4

memcpy:
	lwzu	%r0,	4(%r4)
	stwu	%r0,	4(%r5)
	addic.	%r7,	%r7,	-4
	bne		memcpy

	addi	%r5,	%r5,	4
	sub		%r5,	%r5,	%r6
	srwi	%r7,	%r6,	5
	mtctr	%r7

	li		%r0,	0
DCFlushRange:
	dcbf	%r0,	%r5
	addi	%r5,	%r5,	0x20
	bdnz	DCFlushRange

end:

#Fake IRQ
	
#	lis		%r3,	0xC000

#	li		%r0,	0x20
#	stw		%r0,	0x0054(%r3)

#	li		%r0,	0x40
#	stw		%r0,	0x0050(%r3)

#ARAM Transfer Fake
	li		%r7,	0 #for games which need length 0

SetCount:
	lis		%r3,	AR_DMA_CNT@h
	stw		%r6,	AR_DMA_CNT@l(%r3)

	lwz		%r0,	8(%sp)
	mtlr	%r0
	addi	%sp,	%sp,	0x10

	blr
