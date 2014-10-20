#include <asm.h>

.set	SI_INITED,		0x93002740

SIInitStore:
	lis		%r3,	SI_INITED@h
	li		%r0,	1
	stwu	%r0,	SI_INITED@l(%r3)
	li		%r0,	0
	dcbst	%r0,	%r3
	blr
