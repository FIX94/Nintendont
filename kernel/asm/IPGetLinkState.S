#include <asm.h>

#	r3	srcbuf
#	r4	dstbuf

IPGetLinkState:
	cmplwi %r3, 0
	beq stateOK
	lwz %r3, 4(%r3)
	b storeState
stateOK:
	li %r3, 1
storeState:
	stw %r3, 0(%r4)
	blr
