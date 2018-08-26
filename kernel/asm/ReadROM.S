#include <asm.h>

.set FontBuf, 0x93100000
.set IPL_ROM_FONT_SJIS, 0x1AFF00
.set IPL_ROM_FONT_ANSI, 0x1FCF00
.set IPL_ROM_END_OFFSET, 0x200000

#r3=Dest
#r4=Size
#r5=Source

ReadROM:
	lis		r0,	IPL_ROM_END_OFFSET@h
	ori		r0,	r0,	IPL_ROM_END_OFFSET@l
	cmpw	r5,	r0
	bge		exit
	lis		r0,	IPL_ROM_FONT_SJIS@h
	ori		r0,	r0,	IPL_ROM_FONT_SJIS@l
	cmpw	r5,	r0
	blt		exit

	sub		r5,	r5,	r0

	lis		r6,	FontBuf@h
	ori		r6,	r6,	FontBuf@l
	add		r6,	r6,	r5

	srwi	r0,	r4,	5
	mtctr	r0
	mr		r7,	r6

DCInvalidateRange:
	dcbi	0,	r7
	addi	r7,	r7,	0x20
	bdnz	DCInvalidateRange
	sync

memcpy32prep:
	li		r7,	0
	srwi	r0,	r4,	2
	mtctr	r0
memcpy32:
	lwzx	r0,	r6,	r7
	stwx	r0,	r3,	r7
	addi	r7,	r7,	4
	bdnz	memcpy32

exit:
	blr
