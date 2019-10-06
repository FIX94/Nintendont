#include <asm.h>
.include "constants.inc"

KeyboardRead:
	#to stub instead of arm
	lis		r0,		KeyboardStub@h
	ori		r0,	r0,	KeyboardStub@l
	mtctr	r0
	bctr
