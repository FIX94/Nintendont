#include <asm.h>


StreamRequestVoice:

	mflr    %r0
	stw     %r0, 4(%sp)
	stwu    %sp, -0x60(%sp)
	stmw    %r19, 0x2C(%sp)

	bl		OSDisableInterrupts
	mr		%r21,	%r3

#	AXVPB* AXAcquireVoice	(u32 priority, AXVoiceCallback callback, u32 userContext);

	lis		%r4,	0x800B
	ori		%r4,	%r4,	0xB7D0
	mtctr   %r4

	li		%r3,	15		# NO_DROP
	li		%r4,	0
	li		%r5,	0

	bctrl
	
	lis		%r4,	0x8000
	stw		%r3,	0x18(%r4)
		
# void MIXInitChannel ( AXVPB *p, u32 mode, int input, int auxA, int auxB, int pan, int span, int fader );

	lis		%r4,	0x800B
	ori		%r4,	%r4,	0xF624
	mtctr   %r4

	li		%r4,	0		#	mode
	li		%r5,	0		#	input
	li		%r6,	-904	#	auxA
	li		%r7,	-904	#	auxB	
	li		%r8,	0		#	pan	
	li		%r9,	127		#	span	
	li		%r10,	0		#	fader	

	bctrl

# AXPBADDR
	lis		%r4,	0x8000
	lwz		%r3,	0x18(%r4)		# AXVPB
		
#enable loop
	li		%r0,	1
	sth		%r0,	0x1A6(%r3)
#Format PCM16
	li		%r0,	0x00
	sth		%r0,	0x1A8(%r3)
	
	lis		%r0,	0x20b0
#VoiceLoopAddr 
	stw		%r0,	0x1AA(%r3)
#CurrentAddress
	stw		%r0,	0x1B2(%r3)
#VoiceEndAddress
	lis		%r0,	0x20f0
	stw		%r0,	0x1AE(%r3)

#StartPlay
	li		%r0,	1
	sth		%r0,	0x146(%r3)

#Set 
	lis		%r0,	0x8000	# SYNC_ALL_PARAMS
	stw		%r0,	0x1C(%r3)

end:
	mr		%r3,	%r21
	bl		OSRestoreInterrupts
	
	lmw     %r19, 0x2C(%sp)
	lwz     %r0,	0x64(%sp)
	addi    %sp, %sp, 0x60
	mtlr    %r0
	blr

OSDisableInterrupts:
	mfmsr   %r3
	rlwinm  %r4, %r3, 0,17,15
	mtmsr   %r4
	extrwi  %r3, %r3, 1,16
	blr

OSRestoreInterrupts:
	cmpwi   %r3, 0
	mfmsr   %r4
	beq     loc_8001EA58
	ori     %r5, %r4, 0x8000
	b       loc_8001EA5C
loc_8001EA58:
	rlwinm  %r5, %r4, 0,17,15
loc_8001EA5C:
	mtmsr   %r5
	extrwi  %r3, %r4, 1,16
	blr

