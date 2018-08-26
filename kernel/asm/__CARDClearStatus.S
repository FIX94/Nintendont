#include <asm.h>

#in
#	r3	channel

#out
#	r3 res

__CARDClearStatus:
	li		r3,	0
	blr
