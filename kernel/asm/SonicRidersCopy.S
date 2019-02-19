#include <asm.h>

#r5=size to copy

SonicRidersCopy:
#original instruction
	mr		r31,	r7
#7 might be enough too, going safe
	slwi	r5,	r5,	8
waitLoop:
	subi	r5,	r5,	1
	cmplwi	r5,	0
	bne		waitLoop
	blr
