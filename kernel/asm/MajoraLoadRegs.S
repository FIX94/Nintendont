#little ugly to save instruction
MajoraLoadRegs:
	lwz		%r27,	0xC(%r1)
	lwz		%r26,	0x8(%r1)
	lwz		%r25,	0x4(%r1)
	addi	%r1,	%r1,	0x20
	blr
