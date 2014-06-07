#include <asm.h>

.set fwriteBuff, 0x93160000

#in
#	r3	address
#	r4	size
#	r5	length

#out
#	r3 length written 

patch_fwrite_Log:

 	mullw.  %r4,%r5,%r4
 	ble     end
 	lis     %r7,fwriteBuff@h
 	ori     %r7,%r7,fwriteBuff@l

 	lwz     %r6,4(%r7)
	cmpwi   %r6,0
	bne     end

#DCFlushRange
	srwi	%r6,	%r4,	5
	addi    %r6,    %r6,    2
	mtctr	%r6
	mr		%r6,	%r3
	rlwinm  %r6,    %r6,0,0,26

	li		%r0,	0
DCFlushRange:
	dcbf	%r0,	%r6
	addi	%r6,	%r6,	0x20
	bdnz	DCFlushRange

 	stw     %r3,0(%r7)
 	stw     %r4,4(%r7)

	dcbf    %r0,%r7
 	sync    
ready_loop:
 	lwz     %r6,4(%r7)
	dcbi    %r0,%r7
 	sync    
	cmpw	%r5,	%r6
	beq		ready_loop

end:
 	mr      %r3,%r5
 	blr
