#include <asm.h>

.set	SI_INITED,		0x93003060

SIInitStore:
	lis		r5,	SI_INITED@h
	li		r4,	1
	stwu	r4,	SI_INITED@l(r5)
	li		r4,	0
	dcbf	r4,	r5
	sync
	blr
