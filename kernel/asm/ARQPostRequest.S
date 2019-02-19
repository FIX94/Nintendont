#include <asm.h>

#in
#	r3	request
#	r4	owner	don't care
#	r5	type
#	r6	prio	don't care
#	r7	source
#	r8	dest
#	r9	length
#	r10 cb

#out
#	none

ARQPostRequest:
	stwu    %sp,	-0x10(%sp)
	mflr    r0
	stw     r0,	8(%sp)

## fill updated ARQRequest
	li      r0,	0
	stw		r0,	0x00(r3)	#	struct ARQRequest*  next
	stw		r0,	0x18(r3)	#	u32             length

	stw		r4,	0x04(r3)	#	u32             owner
	stw		r5,	0x08(r3)	#	u32             type
	stw		r6,	0x0C(r3)	#	u32             priority

	add		r0,	r7,	r9
	stw		r0,	0x10(r3)	#	u32             source

	add		r0,	r8,	r9
	stw		r0,	0x14(r3)	#	u32             dest

	stw		r10,	0x1C(r3)	#	ARQCallback     callback

	cmpwi	r9,	0
	beq		callback

	cmpwi	r5,	1
	bne		MRAM_TO_ARAM

ARAM_TO_MRAM:
	rlwinm	r7,	r7,	0,		8,		26 # 0x00ffffe0
	oris	r7,	r7,	0x9000

	rlwinm	r8,	r8,	0,		7,		26 # 0x01ffffe0
	oris	r8,	r8,	0x8000

	b		DCInvalidateRange

MRAM_TO_ARAM:
	rlwinm	r7,	r7,	0,		7,		26 # 0x01ffffe0
	oris	r7,	r7,	0x8000

	rlwinm	r8,	r8,	0,		8,		26 # 0x00ffffe0
	oris	r8,	r8,	0x9000

DCInvalidateRange:
	srwi	r4,	r9,	5
	mtctr	r4
	mr		r4,	r7

DCInvalidateRangeA:
	dcbi	0,	r4
	addi	r4,	r4,	0x20
	bdnz	DCInvalidateRangeA

memcpy_pre:
	mr		r4,	r9		# save length

	lwz		r0,	0(r7)
	stw		r0,	0(r8)
	addic.	r9,	r9,	-4

memcpy:
	lwzu	r0,	4(r7)
	stwu	r0,	4(r8)
	addic.	r9,	r9,	-4
	bne		memcpy

#restore offset
	addi	r8,	r8,	4
	sub		r8,	r8,	r4

#DCFlushRange
	srwi	r4,	r4,	5
	mtctr	r4
	mr		r4,	r8

DCFlushRangeA:
	dcbf	0,	r4
	addi	r4,	r4,	0x20
	bdnz	DCFlushRangeA

callback:
	cmpwi	r10,	0
	beq		end

	mtctr	r10
	bctrl

end:

	lwz		r0,	8(%sp)
	mtlr	r0
	addi	%sp,	%sp,	0x10

	blr
