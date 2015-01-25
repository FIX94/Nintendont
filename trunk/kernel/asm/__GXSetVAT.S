#include <asm.h>

__GXSetVAT:
	lwz		%r9,	0(%r2)	#gets replaced
	li		%r10,	0
	lbz		%r11,	0(%r9)	#gets replaced
	mr		%r8,	%r9
start:
	clrlwi.	%r0,	%r11,	31
	beq		end
	li		%r7,	8
	lis		%r6,	0xCC01
	stb		%r7,	-0x8000(%r6)
	ori		%r0,	%r10,	0x70
	ori		%r4,	%r10,	0x80
	ori		%r3,	%r10,	0x90
	stb		%r0,	-0x8000(%r6)
	li		%r0,	0
	lwz		%r5,	0x1C(%r8)
	stw		%r5,	-0x8000(%r6)
	stb		%r7,	-0x8000(%r6)
	stb		%r4,	-0x8000(%r6)
	lwz		%r4,	0x3C(%r8)
	stw		%r4,	-0x8000(%r6)
	stb		%r7,	-0x8000(%r6)
	stb		%r3,	-0x8000(%r6)
	lwz		%r3,	0x5C(%r8)
	stw		%r3,	-0x8000(%r6)
end:
	stb		%r0,	-0x8000(%r6)
	rlwinm.	%r11,	%r11,	31,		1,	31
	addi	%r10,	%r10,	1
	addi	%r8,	%r8,	4
	bne		start
	li		%r0,	0
	stb		%r0,	0(%r9)	#gets replaced
	blr
