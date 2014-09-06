#include <asm.h>

.set FAKE_EXI_INT,	0xC0000014
.set FAKE_SI_INT,	0xD3026500
.set FAKE_RSW_INT,	0xD3026504

FakeInterrupt:

#check if IPC IRQ
	lwz		%r31,	0x3000(%r3)
	andi.	%r31,	%r31,	0xBFFF
	bne		end

	lis		%r8,	0xCD00
	li		%r3,	0x34
	stw		%r3,	4(%r8)
	lis		%r3,	0x4000
	stw		%r3,	0x30(%r8)

	lis		%r3,	FAKE_SI_INT@h
	lwz		%r0,	FAKE_SI_INT@l(%r3)
	or		%r31,	%r31,	%r0

	lis		%r3,	FAKE_RSW_INT@h
	lwz		%r0,	FAKE_RSW_INT@l(%r3)
	or		%r31,	%r31,	%r0

	lis		%r3,	FAKE_EXI_INT@h
	lwz		%r0,	FAKE_EXI_INT@l(%r3)
	or		%r31,	%r31,	%r0

end:
	lis		%r3,	0xCC00
	blr
