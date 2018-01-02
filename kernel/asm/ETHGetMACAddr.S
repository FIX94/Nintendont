#include <asm.h>

.set _NCDGetMACAddr, 0x9300600C

#	r3	dstbuf

ETHGetMACAddr:
	lis %r4,_NCDGetMACAddr@h
	ori %r4,%r4,_NCDGetMACAddr@l
	mtctr %r4
	bctr
