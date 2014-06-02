.global _start
.type   _start STT_FUNC
.extern __stack_addr
.extern __bss_start
.extern __bss_end
.extern _main
.arm

	.EQU	ios_thread_arg,			5
	.EQU	ios_thread_priority,	0x7F
	.EQU	ios_thread_stacksize,	0x2000

_start:
	@ setup stack
	mov r2,		#0x2000
	ldr sp,		=0x2011C000
	add sp,		sp,				r2

	mov	r5,	r0
	mov	r6,	lr
	
	@ take the plunge
	mov		r0,					r5
	mov		lr,					r6
	mov		r1,					#0
	blx		_main

