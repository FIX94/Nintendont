#include <asm.h>

#
#	r3	chan
#	r4	FileNo
#	r5	FileInfo

CARDFastOpen:

	mflr    %r0
	stw     %r0,	4(%sp)
	stwu    %sp,	-0x10(%sp)	

	cmpwi	%r3,	0
	bne		NoCard
			
#send cmd to DM

	li		%r0,	0
	lis		%r7,	0xC000
	addi	%r7,	%r7,	0x2F60
#IPC area
	dcbi	%r0,	%r7
	addi	%r7,	%r7,	0x20
	dcbi	%r0,	%r7
	
	lis     %r7,	0xC000
	lis     %r0,	0xC500
	
	stw     %r0,	0x2F60(%r7)
	stw     %r4,	0x2F64(%r7)
	stw     %r5,	0x2F68(%r7)	
	
	li      %r0,	3
	stw     %r0,	0x2F78(%r7)
	
ready_loop:
	lwz		%r0,	0x2F78(%r7)
	cmpwi	%r0,	3
	beq		ready_loop

wait_loop:
	lwz		%r0,	0x2F9C(%r7)
	andi.	%r0,	%r0,	0x14
	cmpwi	%r0,	0
	beq		wait_loop
	
	lwz     %r3,	0x2F90(%r7)
	stw     %r3,	0x10(%r5)
	stw     %r4,	0x04(%r5)

	lwz     %r3,	0x2F94(%r7)
	b		end
	
NoCard:
	li		%r3,	-3
end:

	lwz     %r0,	0x14(%sp)
	addi    %sp,	%sp,	0x10
	mtlr    %r0
	blr
