#include <asm.h>

#in
#	r3	channel
#	r4	Status buffer (byte)

#out
#	r3 res 

__CARDReadStatus:

	li		r3,	0x41		# unlock | ready
	stb		r3,	0x0(r4)

	li		r3,	0

	blr
