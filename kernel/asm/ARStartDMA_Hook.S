#include <asm.h>

#in
#	r29	u32 type
#	r28	u32 mainmem_addr
#	r30	u32 aram_addr
#	r31	u32 length

#out
#	none

ARStartDMA:
	cmpwi	%r31,	0
	beq		end

	mr		%r8,	%r28
	rlwinm	%r8,	%r8,	0,		7,		26 # 0x01ffffe0
	oris	%r8,	%r8,	0x8000

	mr		%r9,	%r30
	rlwinm	%r9,	%r9,	0,		8,		26 # 0x00ffffe0
	oris	%r9,	%r9,	0x9000

	cmpwi	%r29,	1
	bne		MRAM_TO_ARAM

ARAM_TO_MRAM:
	mr		%r0,	%r9
	mr		%r9,	%r8
	mr		%r8,	%r0

MRAM_TO_ARAM:		#DCInvalidateRangePrep
	srwi	%r7,	%r31,	5
	mtctr	%r7
	mr		%r7,	%r8

	li		%r0,	0
DCInvalidateRange:
	dcbi	%r0,	%r7
	addi	%r7,	%r7,	0x20
	bdnz	DCInvalidateRange

memcpyPrep:
	mr		%r7,	%r31		#save length
memcpy:
	addic.	%r7,	%r7,	-4
	lwzx	%r0,	%r8,	%r7
	stwx	%r0,	%r9,	%r7
	bne		memcpy

DCFlushRangePrep:
	srwi	%r7,	%r31,	5
	mtctr	%r7

	li		%r0,	0
DCFlushRange:
	dcbf	%r0,	%r9
	addi	%r9,	%r9,	0x20
	bdnz	DCFlushRange

end:
	lis		%r6,	0xCC00
	blr
