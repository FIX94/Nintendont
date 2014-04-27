#include <asm.h>

PADRead:
	#to stub instead of arm
	mflr	%r4
	lis		%r6,	0x9200
	ori		%r6,	%r6,	0x0000
	bl		4
	mflr	%r5 #our point
	addi	%r5,	%r5,	0x40 #to real return 0x10 plus 0x30 buffer
	mtlr	%r6
	blr
	#0x30 buffer
	.int 0; .int 0; .int 0; .int 0; .int 0; .int 0; .int 0; .int 0; .int 0; .int 0; .int 0; .int 0;
	#real return
	mtlr	%r4
	#calculate buffer position
	subi	%r5, 		%r5,		0x30
	#set pointer to real PADStatus
	lis		%r4,	0xD200
	ori		%r4,	%r4,	0x3008
	stw		%r5,		0(%r4)

	#original code
	li		%r4,	0x30

memcpy:
	lwz		%r6,		0(%r5)
	lwz		%r7,		4(%r5)
	lwz		%r8,		8(%r5)
	addi	%r5,		%r5,		0xC
	stw		%r6,		0(%r3)
	stw		%r7,		4(%r3)
	stw		%r8,		8(%r3)
	addi	%r3,		%r3,		0xC
	subi	%r4, 		%r4,		0xC
	cmpwi 	%r4,		0
	bne		memcpy
	blr
