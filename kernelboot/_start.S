.extern _main
.globl _start
_start:
	push {fp,lr}
	bl _main
	pop {fp,lr}
	bx lr
