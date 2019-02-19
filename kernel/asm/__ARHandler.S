#include <asm.h>

.set	AR_MRAM_TO_ARAM,	0
.set	AR_ARAM_TO_MRAM,	1
.set	AR_DMA_MODE,		0x931C0040
.set	AR_DMA_MEM1,		0x931C0044
.set	AR_DMA_MEM2,		0x931C0048
.set	AR_DMA_LEN,			0x931C004C

__ARHandler:
	lis		r3,	AR_DMA_MODE@h
	lwz		r6,	AR_DMA_LEN@l(r3)
	li		r7,	0
	stw		r7,	AR_DMA_LEN@l(r3)

	#adjust ARAM address for safety
	lwz		r5,	AR_DMA_MEM2@l(r3)
	rlwinm	r5,	r5,	0,		6,		26 # 0x03ffffe0
	oris	r5,	r5,	0x9000

	#add length onto ARAM address to see max
	add		r0,	r5,	r6
	#ARAM Max Address to ensure no overwriting 
	lis		r7,	0x9100
	cmplw	r0,	r7
	#Fine as is, continue
	ble		checkLength
	#Use in-bounds area as new length
	subf	r6,	r5,	r7
checkLength:
	#make sure there is any input length
	cmpwi	r6,	0
	ble		end

	#adjust MEM1 address for safety
	lwz		r4,	AR_DMA_MEM1@l(r3)
	rlwinm	r4,	r4,	0,		7,		26 # 0x01ffffe0
	oris	r4,	r4,	0x8000

	lwz		r0,	AR_DMA_MODE@l(r3)
	cmpwi	r0,	AR_MRAM_TO_ARAM
	beq		memcpy32prep

ARAM_TO_MRAM:
	mr		r0,	r4
	mr		r4,	r5
	mr		r5,	r0

memcpy32prep:
	li		r7,	0
	cmplwi	r6,	3
	ble		memcpy16prep
	srwi	r0,	r6,	2	#32bit
	mtctr	r0
memcpy32:
	lwzx	r0,	r4,	r7
	stwx	r0,	r5,	r7
	addi	r7,	r7,	4
	bdnz	memcpy32

memcpy16prep:
	subf	r0,	r7,	r6
	cmpwi	r0,	1
	ble		memcpy8prep
memcpy16:
	lhzx	r0,	r4,	r7
	sthx	r0,	r5,	r7
	addi	r7,	r7,	2

memcpy8prep:
	subf	r0,	r7,	r6
	cmpwi	r0,	0
	ble		DCStoreRangePrep
memcpy8:
	lbzx	r0,	r4,	r7
	stbx	r0,	r5,	r7

DCStoreRangePrep:
	addi	r0,	r6,	0x1F
	srwi	r0,	r0,	5
	mtctr	r0

DCStoreRange:
	dcbst	0,	r5
	addi	r5,	r5,	0x20
	bdnz	DCStoreRange
	sync

end:
	cmplwi	r12,	0
	blr
