#include <asm.h>

#in
#	r3	channel
#	r4	data
#	r5	len
#	r6	mode
#	r7	cb

#out
#	r3 res 

EXIDMA:

	mflr	%r0
	stw		%r0,		4(%sp)
	stwu	%sp,	-0x40(%sp)

	lis		%r8,	0x803F
	stw		%r7,	0x0CB4(%r8)		# callback

	
	lis     %r7,	0xCD80
	lis     %r0,	0x1200
	
	slwi	%r3,	%r3,	20
	slwi	%r6,	%r6,	16

	or		%r0,	%r0,	%r3
	or		%r0,	%r0,	%r5
	or		%r0,	%r0,	%r6
	
	stw     %r4,	0x6010(%r7)
	
## IRQ function call
	lis     %r3,	0x8000
	lwz     %r31,	0xC4(%r3)

	lis		%r8,	0x60
	andc	%r31,	%r31,	%r8

	stw		%r31,	0xC4(%r3)	
		
	stw     %r0,	0x600C(%r7)		
ready_loop:
	lwz		%r3,	0x600C(%r7)
	cmpw	%r3,	%r0
	beq		ready_loop
	
	li		%r3,	1
	
	lwz		%r0,	0x44(%sp)
	addi	%sp,	%sp,		0x40
	mtlr	%r0
	blr
