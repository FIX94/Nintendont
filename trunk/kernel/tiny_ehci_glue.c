/*   
    Copyright (C) 2008 kwiirk.
				  2011-2013  OverjoY

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "string.h"
#include "syscalls.h"
#include "ehci_types.h"
#include "global.h"
#include "ehci.h"

#define static
#define inline extern

#define msleep(a) 			mdelay(a)
#define print_hex_dump_bytes(a, b, c, d)

#define cpu_to_le32(a) 		swab32(a)
#define le32_to_cpu(a) 		swab32(a)
#define cpu_to_le16(a) 		swab16(a)
#define le16_to_cpu(a) 		swab16(a)
#define cpu_to_be32(a) 		(a)
#define be32_to_cpu(a) 		(a)

#define ehci_readl(a)		(*(volatile u32 *)a)
#define ehci_writel(v,a)	do { *(volatile u32 *)a = v; } while(0)

#define ehci_dbg(a...) 		debug_printf(a)
#define printk(a...) 		debug_printf(a)

#define BUG_ON(a) 			if(a)while(1)
#define BUG()				while(1)

#define DUMP_PREFIX_OFFSET 1

struct ehci_hcd _ehci;
struct ehci_hcd *ehci = &_ehci;

#include "ehci.c"

int tiny_ehci_init(void)
{
	int retval;
	ehci = &_ehci;

	ehci->caps = (void*)0x0D040000;
	ehci->regs = (void*)(0x0D040000 + HC_LENGTH(ehci_readl(&ehci->caps->hc_capbase)));

	ehci->num_port = 4;
	ehci->hcs_params = ehci_readl( &ehci->caps->hcs_params );

	/* data structure init */
	retval = ehci_init();
	if(retval)
		return retval; 

	ehci_release_ports(); //quickly release none usb2 port

	return 0;
}
