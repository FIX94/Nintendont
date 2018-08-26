#include <asm.h>

#in
#	r3	channel
#	r4	device
#	r5	freq

#out
#	r3	res

.include "constants.inc"

EXISelect:
	lis		r7,	0xD302
	lis		r0,	0x1000

	slwi	r4,	r4,	8
	slwi	r5,	r5,	16

	or		r0,	r0,	r3
	or		r0,	r0,	r4
	or		r0,	r0,	r5

	stw		r0,	EXI_CMD_0@l(r7)

ready_loop:
	lwz		r3,	EXI_CMD_0@l(r7)
	cmpw	r3,	r0
	beq		ready_loop

	lwz		r3,	EXI_CMD_1@l(r7)
	blr
