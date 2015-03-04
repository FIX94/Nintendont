#include <asm.h>

#r3=obj
#r4=addr
#r5=format
#r6=entries

GXInitTlutObj:
	slwi	%r0,	%r5,	10
	stw		%r0,	0(%r3)
	lis		%r0,	0x6400
	rlwimi	%r0,	%r4,	27, 12, 31	#((addr>>5)&0xFFFFF)
	stw		%r0,	4(%r3)
	sth		%r6,	8(%r3)
	blr
