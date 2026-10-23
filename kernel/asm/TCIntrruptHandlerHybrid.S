#include <asm.h>
.include "constants.inc"

TCIntrruptHandlerHybrid:
	#original instruction
	stw		r0,	4(%sp)

	# Physical channel 1 IRQs (EXI, TC and EXT) must not clear a virtual IRQ.
	cmpwi	r3,	12
	blt		virtual_irq
	cmpwi	r3,	14
	blelr
virtual_irq:
	li		r0,	0
	lis		r8,	INT_BASE@h
	stw		r0,	EXI_INT@l(r8)
	blr
