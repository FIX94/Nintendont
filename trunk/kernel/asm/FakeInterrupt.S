#include <asm.h>

FakeInterrupt:

#check if IPC IRQ
	lwz		%r31,	0x3000(%r3)

	andi.	%r0,	%r31,	0x4000
	cmpwi	%r0,	0
	beq		NoIRQ

	andi.	%r31,	%r31,	0xBFFF
	
	lis		%r3,	0xC000
	lwz		%r0,	0x0014(%r3)
	cmpwi	%r0,	0
	beq		NoIRQ
	
	lis		%r8,	0xCD00
	li		%r3,	0x34
	stw		%r3,	4(%r8)

	lis		%r8,	0xCD00
	lis		%r3,	0x4000
	stw		%r3,	0x30(%r8)

	or		%r31,	%r31,	%r0

NoIRQ:
	lis		%r3,	0xCC00
	blr
