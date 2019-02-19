#include <asm.h>

# - only works for patch_fwrite_A-C
#in
#	r3	address
#	r4	size
#	r5	length

#out - patch_fwrite_A-C
#	r3 length written 

patch_fwrite_GC:
 	mullw.  r4,	r5,	r4
 	ble     end
 	lis     r7,	0xCD00
 	lis     r10,	0xCD00
 	lis     r11,	0xCD00
 	ori     r7,	r7,	0x6418
 	ori     r10,	r10,	0x6428
 	ori     r11,	r11,	0x6424
 	li      r6,	0

outerloop:
 	lbzx    r0,	r6,	r3
 	rlwinm  r0,	r0,	20,0,11
 	oris    r8,	r0,	0xB000

loop:
 	li      r0,	0xD0
 	stw     r0,	0(r7)
 	eieio
 	stw     r8,	0(r10)
 	eieio
 	li      r0,	0x19
 	stw     r0,	0(r11)
 	eieio

NotDone:
 	lwz     r0,	0(r11)
 	sync    
 	andi.   r9,	r0,	1
 	bne     NotDone

 	lwz     r0,	0(r10)
 	sync    
 	li      r9,	0
 	stw     r9,	0(r7)
 	eieio
 	andis.  r9,	r0,	0x400
 	beq     loop

 	addi    r6,	r6,	1
 	cmpw    cr7,	r6,	r4
 	bne     cr7,	outerloop

end:
 	mr      r3,	r5
 	blr
