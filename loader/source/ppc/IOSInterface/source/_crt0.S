
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
	# avetcp shit
	.extern avetcp_init
	.extern avetcp_term
	.extern dns_set_server
	.extern dns_clear_server
	.extern dns_open_addr
	.extern dns_get_addr
	.extern dns_close
	.extern tcp_create
	.extern tcp_bind
	.extern tcp_listen
	.extern tcp_stat
	.extern tcp_getaddr
	.extern tcp_connect
	.extern tcp_accept
	.extern tcp_send
	.extern tcp_receive
	.extern tcp_abort
	.extern tcp_delete
	.extern DHCP_request_nb
	.extern DHCP_get_gateway
	.extern DHCP_get_dns
	.extern DHCP_release
	.extern SOConnect

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
	# avetcp shit
	b avetcp_init
	b avetcp_term
	b dns_set_server
	b dns_clear_server
	b dns_open_addr
	b dns_get_addr
	b dns_close
	b tcp_create
	b tcp_bind
	b tcp_listen
	b tcp_stat
	b tcp_getaddr
	b tcp_connect
	b tcp_accept
	b tcp_send
	b tcp_receive
	b tcp_abort
	b tcp_delete
	b DHCP_request_nb
	b DHCP_get_gateway
	b DHCP_get_dns
	b DHCP_release
	b SOConnect

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

.extern DHCPStateOffset
	.globl setDHCPStateOffset
setDHCPStateOffset:
	lis 4, DHCPStateOffset@ha
	lha 4, DHCPStateOffset@l(4)
	stwx 3, 4, 13
	blr

.extern SOStartedOffset
	.globl setSOStartStatus
setSOStartStatus:
	lis 4, SOStartedOffset@ha
	lha 4, SOStartedOffset@l(4)
	cmplwi 4,0x7777
	beq startstatend
	stwx 3, 4, 13
startstatend:
	blr
