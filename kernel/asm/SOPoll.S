#include <asm.h>
.include "constants.inc"

SOPoll:
	lis 0, SOPollAddr@h
	ori 0, 0, SOPollAddr@l
	mtctr 0
	bctr
