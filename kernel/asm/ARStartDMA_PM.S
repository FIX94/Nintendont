#include <asm.h>

.set	AR_DMA_CNT,			0xCC005028

.set	AR_MRAM_TO_ARAM,	0
.set	AR_ARAM_TO_MRAM,	1

#in
#	r3	u32 type
#	r4	u32 mainmem_addr
#	r5	u32 aram_addr
#	r6	u32 length

#out
#	none

ARStartDMA:
	cmpwi	%r6,	0
	beq		FakeTransfer

	rlwinm	%r4,	%r4,	0,		7,		26 # 0x01ffffe0
	oris	%r4,	%r4,	0x8000

	rlwinm	%r5,	%r5,	0,		8,		26 # 0x00ffffe0
	oris	%r5,	%r5,	0x9000

	cmpwi	%r3,	AR_MRAM_TO_ARAM
	beq		memcpyPrep		#we dont need to invalidate main mem

ARAM_TO_MRAM:
	mr		%r0,	%r4
	mr		%r4,	%r5
	mr		%r5,	%r0

	srwi	%r0,	%r6,	5
	mtctr	%r0
	mr		%r7,	%r4

	li		%r0,	0
DCInvalidateRange:
	dcbi	%r0,	%r7
	addi	%r7,	%r7,	0x20
	bdnz	DCInvalidateRange
	sync

memcpyPrep:
	mr		%r7,	%r6		#save length
memcpy:
	addic.	%r7,	%r7,	-4
	lwzx	%r0,	%r4,	%r7
	stwx	%r0,	%r5,	%r7
	bne		memcpy

DCFlushRangePrep:
	srwi	%r0,	%r6,	5
	mtctr	%r0

	li		%r0,	0
DCFlushRange:
	dcbf	%r0,	%r5
	addi	%r5,	%r5,	0x20
	bdnz	DCFlushRange
	sync

#Early Exception Numbers from:
#https://code.google.com/p/dolphin-emu/source/browse/Source/Core/Core/HW/DSP.cpp#702

#ARAM Transfer Fake
	cmpwi	%r6,	0x20
	bgt		LateIRQ

EarlyIRQ:
	li		%r6,	0
	b		FakeTransfer

LateIRQ:
	srwi	%r6,	%r6,	5	#r6 / 0x20

FakeTransfer:
	lis		%r3,	AR_DMA_CNT@h
	stw		%r6,	AR_DMA_CNT@l(%r3)

	blr
