#include <asm.h>

.set FAKE_EXI_INT,	0xC0000014
.set FAKE_SI_INT,	0xD3026514

FakeInterrupt:

#check if IPC IRQ
	lwz		%r31,	0x3000(%r3)

	lis		%r8,	0xCD00
	lis		%r3,	0x4000
	stw		%r3,	0x30(%r8)

	lis		%r8,	0xCD00
	li		%r3,	0x34
	stw		%r3,	4(%r8)

	andi.	%r31,	%r31,	0xBFFF
	
	lis		%r3,	FAKE_SI_INT@h
	ori		%r3,	%r3,	FAKE_SI_INT@l
	li		%r0,	0x0
	dcbi	%r0,	%r3
	lwz		%r0,	0(%r3)
	or		%r31,	%r31,	%r0
	li		%r0,	0x0
	stw		%r0,	0(%r3)
	dcbf	%r0,	%r3

	lis		%r3,	FAKE_EXI_INT@h
	ori		%r3,	%r3,	FAKE_EXI_INT@l
	li		%r0,	0x0
	dcbi	%r0,	%r3
	lwz		%r0,	0(%r3)
	or		%r31,	%r31,	%r0
	li		%r0,	0x0
	stw		%r0,	0(%r3)
	dcbf	%r0,	%r3

	lis		%r3,	0xCC00
	blr
