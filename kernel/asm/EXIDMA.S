#include <asm.h>

#in
#	r3	channel
#	r4	data
#	r5	len
#	r6	mode
#	r7	cb

#out
#	r3	res

.include "constants.inc"

.set	CH0_MASK,	0x60
.set	CH1_MASK,	0xC
.set	CH_LENGTH,	0x38

EXIDMA:
	lis		r8,	0x803F
	addi		r8,	r8, 0x0CB4
	cmpwi		r3,	1
	bne		CallbackSet
	addi		r8,	r8, CH_LENGTH		#mult instead? break channel2?
CallbackSet:
	stw		r7,	0x4(r8)		# callback

	lis		r8,	CH0_MASK
	cmpwi		r3,	1
	bne		MaskSet
	lis		r8,	CH1_MASK
MaskSet:
	lis		r7,	0xD302
	lis		r0,	0x1200

	slwi	r3,	r3,	20
	slwi	r6,	r6,	16

	or		r0,	r0,	r3
	or		r0,	r0,	r5
	or		r0,	r0,	r6

	stw		r4,	EXI_CMD_1@l(r7)

## IRQ function call
	lis		r3,	0x8000
	lwz		r4,	0xC4(r3)

	andc	r4,	r4,	r8

	stw		r4,	0xC4(r3)

	stw		r0,	EXI_CMD_0@l(r7)
ready_loop:
	lwz		r3,	EXI_CMD_0@l(r7)
	cmpw	r3,	r0
	beq		ready_loop

	li		r3,	1
	blr
