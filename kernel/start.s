.global _start
.type   _start STT_FUNC
.extern __stack_addr
.extern __bss_start
.extern __bss_end
.extern _main
.arm

	.EQU	ios_thread_arg,			5
	.EQU	ios_thread_priority,	255
	.EQU	ios_thread_stacksize,	16384

_start:
	@ setup stack
	mov r2,		#0x4000
	ldr sp,		=0x2010E620
	ldr	r4,		=0xDAA60E0A
	ldr	r4,		=0x2342AAFF
	ldr	r5,		=0x20100000
	add sp,		sp,				r2

	mov	r5,	r0
	mov	r6,	lr
	
	@ take the plunge
	mov		r0,					r5
	mov		lr,					r6
	mov		r1,					#0
	blx		_main

