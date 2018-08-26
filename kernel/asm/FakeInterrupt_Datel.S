#include <asm.h>
.include "constants.inc"

FakeInterrupt:
	# could just use r9 and r28/r10 instead of backup/restore r3 and r31
	mr		r9,	r3
	mr		r28,	r31
	lis		r3,	IRQ_STATUS@h
	ori		r3,	r3,IRQ_STATUS@l
	lwz		r31,	0x0000(r3)
	andi.	r0,	r31,	0x4000	#check if IPC IRQ
	rlwinm	r31,	r31,	0, 18, 15 #relevant irqs # Datel requires bit 15 high
	beq		end		#if bit 14=0 then no need to check

	lis		r3,	0xCD80
	li		r0,	0x22
	stw		r0,	4(r3)
	lis		r0,	0x4000
	stw		r0,	0x30(r3)

	lis		r3,	INT_BASE@h
	lwz		r0,	RSW_INT@l(r3)
	rlwimi	r31,	r0,	0, 30, 30	#0x2

	lwz		r0,	DI_INT@l(r3)
	rlwimi	r31,	r0,	0, 29, 29	#0x4

	lwz		r0,	SI_INT@l(r3)
	rlwimi	r31,	r0,	0, 28, 28	#0x8

	lwz		r0,	EXI_INT@l(r3)
	rlwimi	r31,	r0,	0, 27, 27	#0x10

end:
	mr		r10,	r31
	mr		r3,	r9
	mr		r31,	r28
	mr		r28,	r10 # Some versions use r28, others r10
#	lis		r3,	0xCC00
	blr
