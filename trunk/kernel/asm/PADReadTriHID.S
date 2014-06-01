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
	stw		%r5,	0x3008(%r4)

	#original code
	lwz		%r0,	0(%r5)

	srwi	%r0,	%r0,	16

	li		%r4,	0x0

	andi.   %r5,	%r0,	0x100
	cmpwi	%r5,	0
	beq		ASkip
	ori		%r4,	%r4,	0x6000
ASkip:

	andi.   %r5,	%r0,	0x200
	cmpwi	%r5,	0
	beq		BSkip
	ori		%r4,	%r4,	0x400
BSkip:

	andi.   %r5,	%r0,	0x1000
	cmpwi	%r5,	0
	beq		SSkip
	ori		%r4,	%r4,	0x400
SSkip:

	stw       %r4,	8(%r3)
	stw       %r4,	0xC(%r3)

	addi      %r1,	%r1,	0x20

	blr
