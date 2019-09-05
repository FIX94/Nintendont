#include <asm.h>

.set SOPollAddr,	0x93006034

SOPoll:
	lis 0, SOPollAddr@h
	ori 0, 0, SOPollAddr@l
	mtctr 0
	bctr
