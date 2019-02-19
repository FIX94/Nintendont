#include <asm.h>

GXLoadTlut:
	lwz		r0,	4(r30)
	rlwinm	r0,	r0,	0, 12, 7
	blr
