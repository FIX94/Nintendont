#include <asm.h>

FakeInterrupt:

#check if IPC IRQ
	lwz		%r31,	0x3000(%r3)
	andi.	%r31,	%r31,	0xBFFF

	lis		%r8,	0xCD00
	li		%r3,	0x34
	stw		%r3,	4(%r8)
	lis		%r3,	0x4000
	stw		%r3,	0x30(%r8)

	lis		%r3,	0xCD80

	lwz		%r0,	0x6828(%r3)			#FAKE_EXI_INT
	or		%r31,	%r31,	%r0

	lwz		%r0,	0x6834(%r3)			#FAKE_SI_INT
	or		%r31,	%r31,	%r0

	lwz		%r0,	0x6838(%r3)			#FAKE_RSW_INT
	or		%r31,	%r31,	%r0

end:
	lis		%r3,	0xCC00
	blr
