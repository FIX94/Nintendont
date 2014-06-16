#include <asm.h>

.set FAKE_BITS, 0xC0000014

FakeInterrupt:

#check if IPC IRQ
	lwz		%r31,	0x3000(%r3)

	andi.	%r0,	%r31,	0x4000
	cmpwi	%r0,	0
	beq		NoIRQ

	andi.	%r31,	%r31,	0xBFFF
	
	lis		%r3,	FAKE_BITS@h
	ori		%r3,	%r3,	FAKE_BITS@l
	li		%r0,	0x0
	dcbi	%r0,	%r3
	lwz		%r0,	0(%r3)
	cmpwi	%r0,	0
	beq		NoIRQ
	
	or		%r31,	%r31,	%r0
	li		%r0,	0x0
	stw		%r0,	0(%r3)
	dcbf	%r0,	%r3

	lis		%r8,	0xCD00
	li		%r3,	0x34
	stw		%r3,	4(%r8)

	lis		%r8,	0xCD00
	lis		%r3,	0x4000
	stw		%r3,	0x30(%r8)

NoIRQ:
	lis		%r3,	0xCC00
	blr
