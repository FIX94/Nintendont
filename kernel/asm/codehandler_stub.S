
.set codehandler_bin, 0x800010A0

codehandler_stub:
	#disable interrupts
	mfmsr 4
	rlwinm 0,4,0,17,15
	mtmsr 0
	#save lr on stack
	mflr 0
	stw 0,4(1)
	stwu 1,-0x10(1)
	#save interrupt state
	stw 4,0xC(1)
	#load up codehandler
	lis 4, codehandler_bin@h
	ori 4,4, codehandler_bin@l
	mtctr 4
	bctrl
	#get interrupts back
	lwz 4,0xC(1)
	#get lr back
	addi 1,1,0x10
	lwz 0,4(1)
	mtlr 0
	#restore interrupts
	mtmsr 4
	blr
