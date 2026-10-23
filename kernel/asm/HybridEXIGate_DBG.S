#include <asm.h>
.include "constants.inc"

# Debug-SDK variant of HybridEXIGate. Only r3 and r30 are changed; cr0 and
# XER are preserved because the surrounding code was not written for a call.
#out
#	r3	0xC0000000
#	r30	Slot A status or 0
HybridEXIGate_DBG:
	lis		r3,	INT_BASE@h
	lwz		r30,	EXI_INT@l(r3)
	cntlzw	r30,	r30
	rlwinm	r30,	r30,	27,	31,	31	# 1 when no virtual IRQ is pending
	addi	r30,	r30,	-1			# 0 or 0xFFFFFFFF
	lis		r3,	0xC000
	lwz		r3,	0x10(r3)
	and		r30,	r30,	r3
	lis		r3,	0xC000
	blr
