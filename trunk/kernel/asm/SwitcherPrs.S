#include <asm.h>

.set	PRS_DOL,		0x931C0000
.set	PRS_EXTRACT,	0x931C0020

SwitcherPrs:
	stwu    %sp,	-0x20(%sp)
	mflr    %r0
	stw     %r0,	8(%sp)
	stw     %r31,	0x10(%sp)

	li      %r0,    0
	lis     %r31,   PRS_EXTRACT@h
	addi    %r31,   %r31, PRS_EXTRACT@l
	dcbi    %r0,	%r31
 	sync    
 	lwz     %r0,	0(%r31)
	mtlr	%r0
	lwz		%r31,	0x10(%sp)

	blrl

	stw     %r31,	0x10(%sp)

	# r28 has the dol header at start - make this generic?
	addi    %r31,   %r28, 0xE0   # dol header size =0xE4
FlushHeader:
	dcbf	%r0,	%r31
	subi    %r31,   %r31, 0x20
	cmplw 	%r28,	%r31
	bge		FlushHeader

	lis     %r31,   PRS_DOL@h    # assumes PRS_DOL@l==0
	li      %r0,    0
	stw     %r28,   0(%r31)
	dcbf	%r0,	%r31
 	sync    

ready_loop:
	li      %r0,    0
	dcbi    %r0,	%r31
 	sync    
 	lwz     %r0,	0(%r31)
	cmplw	%r0,	%r28
	beq		ready_loop

	addi    %r31,   %r28, 0xE0   # Update dol entry =0xE0
	li      %r0,    0
	dcbi    %r0,	%r31
	sync    

	lwz		%r31,	0x10(%sp)

	lwz		%r0,	8(%sp)
	mtlr	%r0
	addi	%sp,	%sp,	0x20

end:
	blr
