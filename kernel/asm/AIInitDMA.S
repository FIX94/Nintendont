#include <asm.h>

#in
#	r3	u32 ptr
#	r4	u32 length

#out
#	0xD0B0 0000 

AIInitDMA:
	stwu    %sp,	-0x10(%sp)
	mflr    %r0
	stw     %r0,	8(%sp)

	lis		%r5,	0xC000
	lwz		%r5,	0x14(%r5)	# StreamBufferOffset
	cmpwi	%r5,	0
	beq		end
	
	mr		%r7,	%r4		#save length
	mr		%r8,	%r3		#save ptr

	oris	%r3,	%r3,	0x8000

	lwz		%r0,	0(%r5)
	stw		%r0,	0(%r3)
	addic.	%r4,	%r4,	-4

memcpy:
	lwzu	%r0,	4(%r5)
	stwu	%r0,	4(%r3)
	addic.	%r4,	%r4,	-4
	bne		memcpy

#store new offset
	lis		%r4,	0xC000
	addi	%r5,	%r5,	4
	lwz		%r0,	0x18(%r4)	# StreamBufferENDOffset
	cmpw	%r5,	%r0
	ble		NotDone

	li		%r5,	0
NotDone:
	stw		%r5,	0x14(%r4)	# StreamBufferOffset
	
	oris	%r3,	%r8,	0x8000
	srwi	%r4,	%r7,	5
	mtctr	%r4

	li		%r0,	0
DCFlushRange:
	dcbf	%r0,	%r3
	addi	%r3,	%r3,	0x20
	bdnz	DCFlushRange
	
	mr		%r4,	%r7
	mr		%r3,	%r8

end:
	lwz     %r0, 8(%sp)
	mtlr    %r0
	addi    %sp, %sp, 0x10

	addi	%r30,	%r3,	0

	blr
