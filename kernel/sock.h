/*

Nintendont (Kernel) - Playing Gamecubes in Wii mode on a Wii U

Copyright (C) 2019  FIX94

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/
#ifndef _SOCK_H_
#define _SOCK_H_

void SOCKInit();
void SOCKShutdown();
void SOCKUpdateRegisters();
void SOCKInterrupt(void);

//128KB struct buffer
#define		SO_STRUCT_BUF	0x130E0000
typedef struct _so_struct_arm {
	volatile unsigned int cmd0;
	volatile unsigned int cmd1;
	volatile unsigned int cmd2;
	volatile unsigned int cmd3;
	volatile unsigned int cmd4;
	volatile unsigned int cmd5;
	volatile int busy;
	volatile int retval;
} so_struct_arm;

//used to share location of HSP Interrupt Handler
#define		SO_HSP_LOC		0x13026C80

//all the IOS calls we do
#define so_accept 0x01
#define so_bind 0x02
#define so_close 0x03
#define so_connect 0x04
#define so_fcntl 0x05
#define so_getpeername 0x06
#define so_getsockname 0x07
#define so_setsockopt 0x09
#define so_listen 0x0A
#define so_poll 0x0B
#define so_recvfrom 0x0C
#define so_sendto 0x0D
#define so_shutdown 0x0E
#define so_socket 0x0F
#define so_gethostbyname 0x11
#define so_getinterfaceopt 0x1C
#define so_setinterfaceopt 0x1D
#define so_startinterface 0x1F
//last SO command we use is 0x1F so use 0x20 as flag
#define		SO_NWC_CMD		0x20
#define nwc_startup (SO_NWC_CMD | 6)
#define nwc_cleanup (SO_NWC_CMD | 7)

#define		SO_THREAD_FD	0x40
#endif
