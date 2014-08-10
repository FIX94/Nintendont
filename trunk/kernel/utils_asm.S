.arm

.globl memcpy
.type  memcpy STT_FUNC
.globl memset32
.type  memset32 STT_FUNC 
.globl memset16
.type  memset16 STT_FUNC
.globl memset8
.type  memset8 STT_FUNC
.globl read64
.type  read64 STT_FUNC
.globl write64
.type  write64 STT_FUNC

.text

memcpy:
	cmp		r2,		#0
	bxeq	lr

	stmfd	sp!,	{r4-r6}
	orr		r3,		r0,			r1
	tst		r3,		#3
	bne		_unaligned
	cmp		r2,		#0x0F
	bls		_dwordcopyfast
	cmp		r2,		#0x1F
	bls		_fastcopy16
	stmfd	sp!,	{r7-r9}

_fastcopy32:
	ldmia	r1!,	{r3-r9,r12}
	stmia	r0!,	{r3-r9,r12}
	sub		r2,		r2,			#0x20
	cmp		r2,		#0x1F
	bls		_done
	b		_fastcopy32
_done:
	ldmfd	sp!,	{r7-r9}
	
_fastcopy16:
	cmp		r2,		#0xf
	bls		_dwordcopyfast
	ldmia	r1!,	{r3-r6}
	stmia	r0!,	{r3-r6}
	sub		r2,		r2,			#0x10
	b		_fastcopy16
	
_dwordcopyfast:
	cmp		r2,		#3
	bls		_unaligned
	ldr		r3,		[r1],	#4
	str		r3,		[r0],	#4
	sub		r2,		 r2,	#4
	b		_dwordcopyfast

_unaligned:
	cmp		r0,	#0x1800000
	bcs		_bytecopy
	mov		r12,	#0xff
_dwordcopy:
	cmp		r2,		#0
	beq		_done2
	mov		r6,		r0
	and		r3,		r6,		#3
	ldr		r4,		[r6,-r3]!
	mov		r3,		r3,		lsl #3
	rsb		r3,		r3,		#0x18
	bic		r5,		r4,		r12,	lsl	r3
	ldrb	r4,		[r1],#1
	orr		r4,		r5,		r4,		lsl	r3
	str		r4,		[r6]
	add		r0,		r0,		#1
	sub		r2,		r2,		#1
	b		_dwordcopy

_bytecopy:
	cmp		r2,		#0
	beq		_done2
	ldrb	r3,		[r1],	#1
	strb	r3,		[r0],	#1
	sub		r2,		 r2,	#1
	b		_bytecopy
_done2:
	ldmfd	sp!,	{r4-r6}
	bx		lr
	
memset32:
	bics	r2,		#3
	bxeq	lr
1:	str		r1,		[r0]	,#4
	subs	r2,		#4
	bne		1b
	bx		lr


memset16:
	bics	r2,		#1
	bxeq	lr
1:	strh	r1,		[r0],	#2
	subs	r2,		#2
	bne		1b
	bx		lr

memset8:
	cmp		r2,		#0
	bxeq	lr
1:	strb	r1,		[r0],	#1
	subs	r2,		#1
	bne		1b
	bx		lr

read64:
	ldrd	r0,		[r0]
	bx		lr

write64:
	strd	r2,		[r0]
	bx		lr
