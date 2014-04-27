#include <asm.h>

# r3	ptr
# r4	len
# r5	off

__OSReadROM:

	oris	%r5,	%r5,	0xD100
	
memcpy_pre:	
	lwz		%r0,	0(%r5)
	stw		%r0,	0(%r3)
	addic.	%r4,	%r4,	-4

memcpy:
	lwzu	%r0,	4(%r5)
	stwu	%r0,	4(%r3)
	addic.	%r4,	%r4,	-4
	bne		memcpy

	blr
