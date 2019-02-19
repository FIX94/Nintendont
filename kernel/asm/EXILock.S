#include <asm.h>

#in
#	r3	channel

#out
#	r3 res 

EXILock:

	li		r3,	1
	blr
