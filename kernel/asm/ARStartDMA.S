#include <asm.h>

.set	AR_DMA_CNT,	0xCC005028

#in
#	r3	u32 type
#	r4	u32 mainmem_addr
#	r5	u32 aram_addr
#	r6	u32 length

#out
#	none 

ARStartDMA:
	stwu    %sp,	-0x10(%sp)
	mflr    %r0
	stw     %r0,	8(%sp)

	cmpwi	%r6,	0
	beq		end

	lis		%r7,	0x0FFF
	ori		%r7,	%r7,	0xFFFF
	
	and		%r5,	%r5,	%r7
	addis	%r5,	%r5,	0xD000	# mem2

	cmpwi	%r3,	1
	beq		ARAM_TO_MRAM
	bne		MRAM_TO_ARAM
	
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
	
	cmpwi	%r3,	1
	bne		end

	addi	%r5,	%r5,	4
	sub		%r5,	%r5,	%r7
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
	
	#Command a DMA with 0 size.
	#To use actual size use %r6. (MegaManXCM patches this on the fly)--
	#Assumes no actual data transfer; only affects timing of interrupt.
	#To use 0 
	li		%r7,	0x0
	lis		%r3,	AR_DMA_CNT@h
	stw		%r7,	AR_DMA_CNT@l(%r3)

	lwz     %r0, 8(%sp)
	mtlr    %r0
	addi    %sp, %sp, 0x10

	blr

ARAM_TO_MRAM:
	mr		%r7,	%r5
	mr		%r5,	%r4
	mr		%r4,	%r7

	b memcpy_pre

MRAM_TO_ARAM:

	srwi	%r7,	%r6,	5
	mtctr	%r7
	mr		%r7,	%r4
	
	li		%r0,	0
DCInvalidateRange:
	dcbi	%r0,	%r7
	addi	%r7,	%r7,	0x20
	bdnz	DCInvalidateRange

	b memcpy_pre
