#include <asm.h>

__GXSetVAT:
	lwz		r5,	0(r13)	#GXData
	li		r3,	8
	li		r4,	0
	lbz		r6,	0x4F2(r5)
	mr		r7,	r5		#Backup GXData
	lis		r8,	0xCC01	#GX Register

loop:
	clrlwi.	r0,	r6,	31
	beq		checkNext

	stb		r3,	-0x8000(r8)
	ori		r0,	r4,	0x70
	stb		r0,	-0x8000(r8)
	lwz		r0,	0x1C(r5)
	stw		r0,	-0x8000(r8)

	stb		r3,	-0x8000(r8)
	ori		r0,	r4,	0x80
	stb		r0,	-0x8000(r8)
	lwz		r0,	0x3C(r5)
	stw		r0,	-0x8000(r8)

	stb		r3,	-0x8000(r8)
	ori		r0,	r4,	0x90
	stb		r0,	-0x8000(r8)
	lwz		r0,	0x5C(r5)
	stw		r0,	-0x8000(r8)

checkNext:
	addi	r4,	r4,	1
	addi	r5,	r5,	4
	srwi.	r6,	r6,	1
	bne		loop

	li		r0,	0
	stb		r0,	-0x8000(r8)
	stb		r0,	0x4F2(r7)
	blr
