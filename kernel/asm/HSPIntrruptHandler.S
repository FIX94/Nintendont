#include <asm.h>

.set HSP_INT, 0xD3026580
.set SO_BASE_PPC, 0x93026B80

HSPIntrruptHandler:
	stwu	%sp,	-0x14(%sp)
	mflr	%r0
	stw		%r0,	0xC(%sp)

	# get irq and ack
	lis		4,		HSP_INT@h
	lwz		3,		HSP_INT@l(4)
	li		0,		0
	stw		0,		HSP_INT@l(4)
	# add queue number from irq and execute wakeup
	rlwinm	3,		3,		3,		25,		28
	addis	3,		3,		SO_BASE_PPC@ha
	addi	3,		3,		SO_BASE_PPC@l
	nop		# bl OSWakeupThread

	lwz		%r0,	0xC(%sp)
	addi	%sp,	%sp,	0x14
	mtlr	%r0
	blr
