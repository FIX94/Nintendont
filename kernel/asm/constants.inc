/*
Constants for Nintendont ARM assembler code.
Copyright (c) 2014-2017 FIX94 and Nintendont contributors.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __NINTENDONT_KERNEL_ASM_CONSTANTS_H__
#define __NINTENDONT_KERNEL_ASM_CONSTANTS_H__

/** Configuration **/

.set	NinCfg,			0xD3004000
.set	NinCfgConfig,		NinCfg + 0x8

.set	NIN_CFG_MC_SLOTB,	0x80000000

/** EXI registers **/

.set	EXI_CMD_0,		0xD3026800
.set	EXI_CMD_1,		0xD3026804

/** Interrupts **/

.set	IRQ_STATUS,		0xCC003000

/* all in different cache blocks */

.set	INT_BASE,		0xD3026500
.set	RSW_INT,		INT_BASE+0x00
.set	DI_INT,			INT_BASE+0x20
.set	SI_INT,			INT_BASE+0x40
.set	EXI_INT,		INT_BASE+0x60
.set	HSP_INT,		INT_BASE+0x80

/** Audio streaming **/

.set	STREAM_BASE,	0xD30265A0
.set	STREAMING,		STREAM_BASE+0x00
.set	UPDATE_STREAM,	STREAM_BASE+0x20
.set	AI_ADP_LOC,		STREAM_BASE+0x40

.set	AI_ADP_BUF,		0x93280000

.set	AI_CR,			0xCD806C00
.set	AI_VR,			0xCD806C04

.set	AI_DMA_ADDR,	0xCC005030
.set	AI_DMA_SIZE,	0xCC005036

/** Socket Interface **/

.set	SO_BASE_PPC,	0x93026900

.set	IOSInterfaceAddr,			0x93006000
.set	SOInitAddr,					IOSInterfaceAddr+0x00
.set	SOStartupAddr,				IOSInterfaceAddr+0x04
.set	SOCleanupAddr,				IOSInterfaceAddr+0x08
.set	SOSocketAddr,				IOSInterfaceAddr+0x0C
.set	SOCloseAddr,				IOSInterfaceAddr+0x10
.set	SOListenAddr,				IOSInterfaceAddr+0x14
.set	SOAcceptAddr,				IOSInterfaceAddr+0x18
.set	SOBindAddr,					IOSInterfaceAddr+0x1C
.set	SOShutdownAddr,				IOSInterfaceAddr+0x20
.set	SORecvFromAddr,				IOSInterfaceAddr+0x24
.set	SOSendToAddr,				IOSInterfaceAddr+0x28
.set	SOSetSockOptAddr,			IOSInterfaceAddr+0x2C
.set	SOFcntlAddr,				IOSInterfaceAddr+0x30
.set	SOPollAddr,					IOSInterfaceAddr+0x34

.set	IPGetMacAddrAddr,			IOSInterfaceAddr+0x38
.set	IPGetNetmaskAddr,			IOSInterfaceAddr+0x3C
.set	IPGetAddrAddr,				IOSInterfaceAddr+0x40
.set	IPGetMtuAddr,				IOSInterfaceAddr+0x44
.set	IPGetLinkStateAddr,			IOSInterfaceAddr+0x48
.set	IPGetConfigErrorAddr,		IOSInterfaceAddr+0x4C
.set	IPSetConfigErrorAddr,		IOSInterfaceAddr+0x50
.set	IPClearConfigErrorAddr,		IOSInterfaceAddr+0x54

.set	avetcp_init_Addr,			IOSInterfaceAddr+0x58
.set	avetcp_term_Addr,			IOSInterfaceAddr+0x5C

.set	dns_set_server_Addr,		IOSInterfaceAddr+0x60
.set	dns_clear_server_Addr,		IOSInterfaceAddr+0x64
.set	dns_open_addr_Addr,			IOSInterfaceAddr+0x68
.set	dns_get_addr_Addr,			IOSInterfaceAddr+0x6C
.set	dns_close_Addr,				IOSInterfaceAddr+0x70

.set	tcp_create_Addr,			IOSInterfaceAddr+0x74
.set	tcp_bind_Addr,				IOSInterfaceAddr+0x78
.set	tcp_listen_Addr,			IOSInterfaceAddr+0x7C
.set	tcp_stat_Addr,				IOSInterfaceAddr+0x80
.set	tcp_getaddr_Addr,			IOSInterfaceAddr+0x84
.set	tcp_connect_Addr,			IOSInterfaceAddr+0x88
.set	tcp_accept_Addr,			IOSInterfaceAddr+0x8C
.set	tcp_send_Addr,				IOSInterfaceAddr+0x90
.set	tcp_receive_Addr,			IOSInterfaceAddr+0x94
.set	tcp_abort_Addr,				IOSInterfaceAddr+0x98
.set	tcp_delete_Addr,			IOSInterfaceAddr+0x9C

.set	DHCP_request_nb_Addr,		IOSInterfaceAddr+0xA0
.set	DHCP_get_gateway_Addr,		IOSInterfaceAddr+0xA4
.set	DHCP_get_dns_Addr,			IOSInterfaceAddr+0xA8
.set	DHCP_release_Addr,			IOSInterfaceAddr+0xAC
.set 	SOConnectAddr,				IOSInterfaceAddr+0xB0

/** Disc Interface **/

.set	DI_BASE,		0xD3026000
.set	DI_INV_ADR,		DI_BASE+0x28
.set	DI_INV_LEN,		DI_BASE+0x2C

/** Triforce **/

.set	IN_TESTMENU,	0xD3003080	/* uncached for kernel */

/** PAD redirection **/

.set	PadStub,		0x93000000
.set	KeyboardStub,	0x93000004
.set	PadBuff,		0x93003100

.set	PadRumble,		0x93003010
.set	PadRumble_0,	PadRumble+0x00
.set	PadRumble_1,	PadRumble+0x04
.set	PadRumble_2,	PadRumble+0x08
.set	PadRumble_3,	PadRumble+0x0C

#endif /* __NINTENDONT_KERNEL_ASM_CONSTANTS_H__ */
