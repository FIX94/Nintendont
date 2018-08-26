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

EXIImm:
	mflr	r0
	stw		r0,	4(%sp)
	stwu	%sp,	-0x40(%sp)

	mr		r8,	r7

	lis		r7,	0xD302

	lis		r0,	0x1100

	slwi	r3,	r3,	20
	slwi	r6,	r6,	16

	or		r0,	r0,	r3
	or		r0,	r0,	r5
	or		r0,	r0,	r6

	cmpwi	r6,	0
	beq		LdPtr

	cmpwi	r5,	4
	bgt		LdPtr

	lwz		r3,	0(r4)
	stw		r3,	EXI_CMD_1@l(r7)
	b		NPtr

LdPtr:
	stw		r4,	EXI_CMD_1@l(r7)
NPtr:
	stw		r0,	EXI_CMD_0@l(r7)
ready_loop:
	lwz		r3,	EXI_CMD_0@l(r7)
	cmpw	r3,	r0
	beq		ready_loop

	cmpwi	r6,	0
	bne		end

CopyResult:
	lwz		r3,	EXI_CMD_1@l(r7)
	cmpwi	r5,	1
	beq		ByteWrite
	cmpwi	r5,	2
	beq		HalfWrite
	cmpwi	r5,	4
	beq		WordWrite
	b		end
ByteWrite:
	stb		r3,	0(r4)
	b		end
HalfWrite:
	sth		r3,	0(r4)
	b		end
WordWrite:
	stw		r3,	0(r4)
	b		end

end:
	cmpwi	r8,	0
	beq		NoCB

	mtctr	r8
	li		r3,	0
	li		r4,	0
	bctrl

NoCB:
	li		r3,	1

	lwz		r0,	0x44(%sp)
	addi	%sp,	%sp,	0x40
	mtlr	r0
	blr
