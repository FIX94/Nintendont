
	# full list of the functions we can jump to
	.extern PADRead
	.extern KeyboardRead

	.globl __start
__start:
	b PADRead
	b KeyboardRead

# just some helper functions
	.globl disableIRQs
disableIRQs:
	mfmsr 3
	rlwinm 4,3,0,17,15
	mtmsr 4
	extrwi 3,3,1,16
	blr

	.globl restoreIRQs
restoreIRQs:
	cmpwi 3,0
	mfmsr 4
	beq doRestDisable
	# re-enable interrupts
	ori 5,4,0x8000
	b restoreSetVal
doRestDisable:
	rlwinm 5,4,0,17,15
restoreSetVal:
	mtmsr 5
	extrwi 3,4,1,16
	blr

	.globl disableCaches
disableCaches:
	sync
	isync
	mfspr 3,1008
	rlwinm 3,3,0,18,15
	mtspr 1008,3
	blr

	.globl bootStub
bootStub:
	lis 3, 0x8000
	ori 3, 3, 0x4000
	mtlr 3
	blr
