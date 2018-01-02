
	.extern printRetAddr
	.extern SOStartup
	.extern SOGetHostId
	.extern SOGetInterfaceOpt
	.extern NCDGetMACAddr

	.globl __start
	.globl _SOStartup
	.globl _SOGetHostId
	.globl _SOGetInterfaceOpt
	.globl _NCDGetMACAddr

__start:
	b printRetAddr
_SOStartup:
	b SOStartup
_SOGetHostId:
	b SOGetHostId
_SOGetInterfaceOpt:
	b SOGetInterfaceOpt
_NCDGetMACAddr:
	b NCDGetMACAddr
