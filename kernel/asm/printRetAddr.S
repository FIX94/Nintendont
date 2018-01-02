#include <asm.h>

.set SO_IOCTL,	0xD3026900
.set SO_CMD_0,	0xD3026908

printRetAddr:
	lis		%r4,	SO_IOCTL@h
	stw		%r0,	SO_CMD_0@l(%r4)
	li		%r0,	0xFF
	stw		%r0,	SO_IOCTL@l(%r4)
wait:
	lwz		%r0,	SO_IOCTL@l(%r4)
	cmpwi	%r0,	0
	bne		wait
	blr
