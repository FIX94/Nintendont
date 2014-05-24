
#include <asm.h>

#in
#	r3	cmdstruct
#	r4	offset
#	r5	cb

#out
#	r3 res 


DVDSeekAbsAsyncPrio:
 	stwu    %sp,-0x10(%sp)
 	mflr    %r0
 	stw     %r0,0x08(%sp)
 	li      %r0,0
 	stw     %r0,0x00(%r3)
 	stw     %r0,0x04(%r3)
 	stw     %r0,0x1C(%r3)
 	li      %r0,2
 	stw     %r0,0x08(%r3)
 	li      %r0,0
 	stw     %r0,0x0C(%r3)
 	stw     %r4,0x10(%r3)
 	stw     %r5,0x28(%r3)
 	cmpwi   %r5,0
 	beq     end
 	mtctr   %r5
 	mr      %r4,%r3
 	li      %r3,0
 	bctrl
end:
 	li      %r3,1
 	lwz     %r0,0x08(%sp)
 	mtlr    %r0
 	addi    %sp,%sp,0x10
 	blr
