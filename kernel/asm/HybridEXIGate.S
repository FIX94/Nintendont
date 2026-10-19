#include <asm.h>
.include "constants.inc"

# Replaces the Slot A status load in __OSDispatchInterrupt.
# The synthetic status word is only valid while a virtual EXI IRQ is pending;
# physical Slot B IRQs must not dispatch a stale Slot A status.
# Only r3, r4 and cr0 are changed; the caller recomputes cr0.
#out
#	r3	0xC0000000
#	r4	Slot A status or 0
HybridEXIGate:
	lis		r3,	INT_BASE@h
	lwz		r4,	EXI_INT@l(r3)
	lis		r3,	0xC000
	cmpwi	r4,	0
	beqlr
	lwz		r4,	0x10(r3)
	blr
