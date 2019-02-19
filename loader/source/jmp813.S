
	.extern __InitBATS
	.extern __InitPS
	.extern __InitFPRS
	.extern __InitCache
	.extern __InitSystem

	.globl _jmp813
_jmp813:
	bl		__InitBATS			# Initialize BATs to a clear and known state
	bl		__initRegisters		# Initialize the General Purpose Registers
	bl		__InitPS 			# Initialize Paired Singles
	bl		__InitFPRS		 	# Initialize the FPR's
	bl		__InitCache 		# Initialize the system caches
	bl		__InitSystem		# Initialize more cache aspects, clear a few SPR's, and disable interrupts.
	lis		r3, 0x8130
	mtlr	r3
	blr

__initRegisters:
	li      r0,0
	li      r3,0
	li      r4,0
	li      r5,0
	li      r6,0
	li      r7,0
	li      r8,0
	li      r9,0
	li      r10,0
	li      r11,0
	li      r12,0
	li      r14,0
	li      r15,0
	li      r16,0
	li      r17,0
	li      r18,0
	li      r19,0
	li      r20,0
	li      r21,0
	li      r22,0
	li      r23,0
	li      r24,0
	li      r25,0
	li      r26,0
	li      r27,0
	li      r28,0
	li      r29,0
	li      r30,0
	li      r31,0
	blr
