
	# full list of the functions we can jump to
	.extern SOInit
	.extern SOStartup
	.extern SOCleanup
	.extern IPGetMacAddr
	.extern IPGetNetmask
	.extern IPGetAddr
	.extern IPGetMtu
	.extern IPGetLinkState
	.extern IPGetConfigError
	.extern IPSetConfigError
	.extern IPClearConfigError
	.extern SOSocket
	.extern SOClose
	.extern SOListen
	.extern SOAccept
	.extern SOBind
	.extern SOShutdown
	.extern SORecvFrom
	.extern SOSendTo
	.extern SOSetSockOpt
	.extern SOFcntl
	.extern SOPoll

	.globl __start
__start:
	# redirected SO calls
	b SOInit
	b SOStartup
	b SOCleanup
	b SOSocket
	b SOClose
	b SOListen
	b SOAccept
	b SOBind
	b SOShutdown
	b SORecvFrom
	b SOSendTo
	b SOSetSockOpt
	b SOFcntl
	b SOPoll
	# emulated IP calls
	b IPGetMacAddr
	b IPGetNetmask
	b IPGetAddr
	b IPGetMtu
	b IPGetLinkState
	b IPGetConfigError
	b IPSetConfigError
	b IPClearConfigError


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

.set SO_HSP_LOC,		0x93026C80
	.globl setSOStartStatus
setSOStartStatus:
	lis 4, SO_HSP_LOC@h
	ori 4, 4, SO_HSP_LOC@l
	lha 4, 8(4)
	stwx 3, 4, 13
	blr
