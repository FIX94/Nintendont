#they left the stack room but didnt use it, good for us
MajoraSaveRegs:
	stw		%r28,	0x10(%r1)
	stw		%r27,	0xC(%r1)
	stw		%r26,	0x8(%r1)
	stw		%r25,	0x4(%r1)
	nop		#b ori
