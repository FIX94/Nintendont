#include <asm.h>

.set	AR_DMA_SAVE,		0x931C0040

#in
#	r29	u32 type
#	r28	u32 mainmem_addr
#	r30	u32 aram_addr
#	r31	u32 length

#out
#	none

ARStartDMA:
	lis		%r6,	AR_DMA_SAVE@h
	stwu	%r29,	AR_DMA_SAVE@l(%r6)
	stwu	%r28,	4(%r6)
	stwu	%r30,	4(%r6)
	stw		%r31,	4(%r6)

	lis		%r6,	0xCC00
	blr
