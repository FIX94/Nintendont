#include <asm.h>

FakeInterrupt:

#check if IPC IRQ
	lwz		%r29,	0x3000(%r3)
	andi.	%r29,	%r29,	0xBFFF

	lis		%r3,	0xCD80
	li		%r0,	0x34
	stw		%r0,	4(%r3)
	lis		%r0,	0x4000
	stw		%r0,	0x30(%r3)

	lwz		%r0,	0x6828(%r3)			#FAKE_EXI_INT
	or		%r29,	%r29,	%r0

	lwz		%r0,	0x6834(%r3)			#FAKE_SI_INT
	or		%r29,	%r29,	%r0

	lwz		%r0,	0x6838(%r3)			#FAKE_RSW_INT
	or		%r29,	%r29,	%r0

end:
	lis		%r3,	0xCC00
	blr
