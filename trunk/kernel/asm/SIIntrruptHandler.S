#include <asm.h>

.set FAKE_SI_INT,	0xD3026500

SIIntrruptHandler:

	stw		%r0, 4(%sp)
	
	lis		%r8,	0xCD00
	li		%r0,	0x34
	stw		%r0,	4(%r8)

	lis		%r8,	FAKE_SI_INT@h
	li		%r0,	0x0
	stw		%r0,	FAKE_SI_INT@l(%r8)

	lis		%r8,	0xCD00
	lis		%r0,	0x4000
	stw		%r0,	0x30(%r8)

	blr
