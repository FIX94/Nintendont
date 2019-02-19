#include <asm.h>

#.set	DATEL_TIMER,		0x00013800 # 0x6800 * 3
.set	DATEL_TIMER,		0x00009C00 # 0x6800 * 3 / 2

DatelTimer:
	li		r0,	DATEL_TIMER@h
	ori		r0,	r0, DATEL_TIMER@l
	mtctr	r0
DatelTimerWait:
	bdnz	DatelTimerWait

	blr
