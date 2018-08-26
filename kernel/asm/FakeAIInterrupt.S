#include <asm.h>

FakeAIInterrupt:

	lis		r3,	0xC000
	lwz		r4,	0x0054(r3)
	li		r0,	0
	stw		r0,	0x0054(r3)

	lis		r3,	0xCC00
	lhz		r0,	0x500A(r3)

	or		r4,	r0,	r4
	
	blr
