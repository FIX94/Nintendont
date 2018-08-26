#include <asm.h>

.set fwriteBuff, 0x93160000

#in - patch_fwrite_A-C
#	r3	address
#	r4	size
#	r5	length
#in - patch_fwrite_D
#	r3	address
#	r4	length

#out - patch_fwrite_A-C
#	r3 length written 
#out - patch_fwrite_D
#	r3 ret =0 

patch_fwrite_Log:

# 	mr.     r5,	r4		# - use instead for patch_fwrite_D
 	mullw.  r4, r5, r4
 	ble     end
 	lis     r7, fwriteBuff@h
 	ori     r7, r7, fwriteBuff@l

 	lwz     r6, 4(r7)
	cmpwi   r6, 0
	bne     end

#DCFlushRange
	srwi	r6, r4, 5
	addi    r6, r6, 2
	mtctr	r6
	mr		r6, r3
	rlwinm  r6, r6, 0,0,26

DCFlushRange:
	dcbf	0, r6
	addi	r6, r6, 0x20
	bdnz	DCFlushRange

 	stw     r3, 0(r7)
 	stw     r4, 4(r7)

	dcbf    0, r7
 	sync    
ready_loop:
 	lwz     r6,	4(r7)
	dcbi    0, r7
 	sync    
	cmpw	r4, r6
	beq		ready_loop

end:
# 	li      r3,	0	#- use instead for patch_fwrite_D
 	mr      r3, r5
 	blr
