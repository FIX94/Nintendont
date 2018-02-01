#include <asm.h>

#r5=size to copy

SonicRidersCopy:
#original instruction
	mr		%r31,	%r7
#6 works in story mode 100%, 5 crashes after third race
	slwi	%r5,	%r5,	6
waitLoop:
	subi	%r5,	%r5,	1
	cmplwi	%r5,	0
	bne		waitLoop
	blr
