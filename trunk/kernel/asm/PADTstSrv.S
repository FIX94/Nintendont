#include <asm.h>


PADTstSrv:

	lis		%r4,	0xCD00
	lwz		%r0,	0x6404(%r4)
	lwz		%r4,	0x6408(%r4)

	srwi	%r0,	%r0,	20
	andi.   %r0,	%r0,	0xC0

	not		%r0,	%r0

	andi.   %r3,	%r0,	0xFF

	blr
