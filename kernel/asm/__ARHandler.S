#include <asm.h>

.set	AR_MRAM_TO_ARAM,	0
.set	AR_ARAM_TO_MRAM,	1
.set	AR_DMA_SAVE,		0x931C0040

__ARHandler:
	lis		%r6,	AR_DMA_SAVE@h
	lwzu	%r0,	AR_DMA_SAVE@l(%r6)

	lwzu	%r4,	4(%r6)
	lwzu	%r5,	4(%r6)
	lwz		%r6,	4(%r6)

	cmpwi	%r6,	0
	beq		end

	rlwinm	%r4,	%r4,	0,		7,		26 # 0x01ffffe0
	oris	%r4,	%r4,	0x8000

	rlwinm	%r5,	%r5,	0,		8,		26 # 0x00ffffe0
	oris	%r5,	%r5,	0x9000

	cmpwi	%r0,	AR_MRAM_TO_ARAM
	beq		memcpyPrep

ARAM_TO_MRAM:
	mr		%r0,	%r4
	mr		%r4,	%r5
	mr		%r5,	%r0

memcpyPrep:		#save length for DCStoreRange
	srwi	%r0,	%r6,	5
	mtctr	%r0

memcpy:
	addic.	%r6,	%r6,	-4
	lwzx	%r0,	%r4,	%r6
	stwx	%r0,	%r5,	%r6
	bne		memcpy

	li		%r0,	0
DCStoreRange:
	dcbst	%r0,	%r5
	addi	%r5,	%r5,	0x20
	bdnz	DCStoreRange
	sync

end:
	cmplwi	%r12,	0
	blr
