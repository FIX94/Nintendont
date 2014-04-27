/* simplest usb-ehci driver which features:

   control and bulk transfers only
   only one transfer pending
   driver is synchronous (waiting for the end of the transfer)
   endianess independant
   no uncached memory allocation needed

   this driver is originally based on the GPL linux ehci-hcd driver

 * Original Copyright (c) 2001 by David Brownell
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/* magic numbers that can affect system performance */
#define	EHCI_TUNE_CERR		0	/* 0-3 qtd retries; 0 == don't stop */
#define	EHCI_TUNE_RL_HS		4	/* nak throttle; see 4.9 */
#define	EHCI_TUNE_RL_TT		0
#define	EHCI_TUNE_MULT_HS	1	/* 1-3 transactions/uframe; 4.10.3 */
#define	EHCI_TUNE_MULT_TT	1
#define	EHCI_TUNE_FLS		2	/* (small) 256 frame schedule */

#undef DEBUG

static void dbg_qtd (const char *label, struct ehci_qtd *qtd)
{
	//ehci_dbg( "%s td %p n%08x %08x t%08x p0=%08x\n", label, qtd, 
	//	hc32_to_cpup( &qtd->hw_next),
	//	hc32_to_cpup( &qtd->hw_alt_next),
	//	hc32_to_cpup( &qtd->hw_token),
	//	hc32_to_cpup( &qtd->hw_buf [0]));
	//if (qtd->hw_buf [1])
	//	ehci_dbg( "  p1=%08x p2=%08x p3=%08x p4=%08x\n",
	//		hc32_to_cpup( &qtd->hw_buf[1]),
	//		hc32_to_cpup( &qtd->hw_buf[2]),
	//		hc32_to_cpup( &qtd->hw_buf[3]),
	//		hc32_to_cpup( &qtd->hw_buf[4]));
}

static void dbg_qh (const char *label, struct ehci_qh *qh)
{
	//ehci_dbg ( "%s qh %p n%08x info %x %x qtd %x\n", label,
	//	qh, 
	//	hc32_to_cpu(qh->hw_next), 
	//	hc32_to_cpu(qh->hw_info1), 
	//	hc32_to_cpu(qh->hw_info2),
	//	hc32_to_cpu(qh->hw_current));
	//dbg_qtd ("overlay",  (struct ehci_qtd *) &qh->hw_qtd_next);
}

static void dbg_command (void)
{
#ifdef DEBUG
	u32 command=ehci_readl( &ehci->regs->command);
	u32 async=ehci_readl( &ehci->regs->async_next);
		
	ehci_dbg ("async_next: %08x\n",async);
	ehci_dbg (
		"command %06x %s=%d ithresh=%d%s%s%s%s %s %s\n",
		 command,
		(command & CMD_PARK) ? "park" : "(park)",
		CMD_PARK_CNT (command),
		(command >> 16) & 0x3f,
		(command & CMD_LRESET) ? " LReset" : "",
		(command & CMD_IAAD) ? " IAAD" : "",
		(command & CMD_ASE) ? " Async" : "",
		(command & CMD_PSE) ? " Periodic" : "",
		(command & CMD_RESET) ? " Reset" : "",
		(command & CMD_RUN) ? "RUN" : "HALT"
	);
#endif
}

static void dbg_status (void)
{
#ifdef DEBUG
	u32 status=ehci_readl( &ehci->regs->status);
	ehci_dbg (
	 "status %04x%s%s%s%s%s%s%s%s%s%s\n",
		status,
		(status & STS_ASS) ? " Async" : "",
		(status & STS_PSS) ? " Periodic" : "",
		(status & STS_RECL) ? " Recl" : "",
		(status & STS_HALT) ? " Halt" : "",
		(status & STS_IAA) ? " IAA" : "",
		(status & STS_FATAL) ? " FATAL" : "",
		(status & STS_FLR) ? " FLR" : "",
		(status & STS_PCD) ? " PCD" : "",
		(status & STS_ERR) ? " ERR" : "",
		(status & STS_INT) ? " INT" : ""
	);
#endif
}

void debug_qtds(void)
{
	struct ehci_qh *qh = ehci->async; 
	struct ehci_qtd *qtd;
	dbg_qh ("qh",qh);
	dbg_command ();
	dbg_status ();
	for(qtd = qh->qtd_head; qtd; qtd = qtd->next)
	{
		ehci_dma_unmap_bidir(qtd->qtd_dma,sizeof(struct ehci_qtd));
		dbg_qtd("qtd",qtd);
		ehci_dma_map_bidir(qtd,sizeof(struct ehci_qtd));
	}
}

void dump_qh(struct ehci_qh	*qh)
{
	struct ehci_qtd	*qtd;
	dbg_command ();
	dbg_status ();
	ehci_dma_unmap_bidir(qh->qh_dma,sizeof(struct ehci_qh));
	dbg_qh("qh",qh);
	print_hex_dump_bytes("qh:",DUMP_PREFIX_OFFSET,(void*)qh,12*4);
	for(qtd = qh->qtd_head; qtd; qtd = qtd->next)
	{
		u32 *buf;
		ehci_dma_unmap_bidir(qtd->qtd_dma,sizeof(struct ehci_qtd));
		dbg_qtd("qtd",qtd);
		print_hex_dump_bytes("qtd:",DUMP_PREFIX_OFFSET,(void*)qtd,8*4);
		buf = (u32*)hc32_to_cpu(qtd->hw_buf[0]);
		if(buf)
			print_hex_dump_bytes("qtd buf:",DUMP_PREFIX_OFFSET,(void*)(buf),8*4);

	}
}

/*-------------------------------------------------------------------------*/

/*
 * handshake - spin reading hc until handshake completes or fails
 * @ptr: address of hc register to be read
 * @mask: bits to look at in result of read
 * @done: value of those bits when handshake succeeds
 * @usec: timeout in microseconds
 *
 * Returns negative errno, or zero on success
 *
 * Success happens when the "mask" bits have the specified value (hardware
 * handshake done).  There are two failure modes:  "usec" have passed (major
 * hardware flakeout), or the register reads as all-ones (hardware removed).
 *
 * That last failure should_only happen in cases like physical cardbus eject
 * before driver shutdown. But it also seems to be caused by bugs in cardbus
 * bridge shutdown:  shutting down the bridge before the devices using it.
 */
 
 #include "common.h"
 
int unplug_device = 0;

#define INTR_MASK (STS_IAA | STS_FATAL | STS_PCD | STS_ERR | STS_INT)

int handshake_mode = 0;

u32 usb_timeout= 1000 * 1000;

#include "global.h"
#define get_timer()  (*(((vu32*)HW_TIMER)))
 
static int handshake(void __iomem *pstatus, void __iomem *ptr, u32 mask, u32 done, int usec)
{
	u32	result, status, g_status,g_mstatus, ret = 0;
	
	u32 tmr, temp;

    tmr = get_timer();
	usec<<=1;

	do 
	{
		udelay(10);
		status = ehci_readl( pstatus);
		result = ehci_readl( ptr);
		g_status=ehci_readl(&ehci->regs->status);
		g_mstatus=g_status & INTR_MASK;
		
		if((g_status == ~(u32)0) || (PORT_OWNER&status) || (g_status & STS_FATAL))		/* card removed */
		{
			unplug_device=1;
			return -ENODEV;
		}
		
		if(!(PORT_CONNECT&status))		/* card removed */
		{
			unplug_device = 1;
			ret = -ENODEV;
			goto handshake_exit;
		}
		result &= mask;
		
		udelay(100);
		result = ehci_readl(ptr);
		if(result == ~(u32)0)		/* card removed */
			return -ENODEV;
			
		result &= mask;
		
		if(g_status & STS_ERR) 
		{
			if(handshake_mode) 
				ret=-ETIMEDOUT; 
			else 
			{
				unplug_device = 1;
				ret = -ENODEV;
			}
			goto handshake_exit;

		}
		
		if(result == done)
		{
			ret=0;
			goto handshake_exit;
		}
			
		
		temp = get_timer() - tmr;
	} while(temp < usec);
	
	if(handshake_mode) 
		ret = -ETIMEDOUT;
	else
	{
		unplug_device=1;
		ret=-ENODEV;
	}
	
handshake_exit:	

	ehci_writel(g_mstatus, &ehci->regs->status);
	/* complete the unlinking of some qh [4.15.2.3] */
	u32 command = ehci_readl( &ehci->regs->command);
	if(g_status & STS_IAA) 
	{
		/* guard against (alleged) silicon errata */
		if(command & CMD_IAAD) 
			ehci_writel(command & ~CMD_IAAD, &ehci->regs->command);
	}
		
	return ret;	
}

#include "ehci-mem.c"
/* one-time init, only for memory state */
static int ehci_init(void)
{
	int retval;
 	if((retval = ehci_mem_init()) < 0)
		return retval;
	/*
	 * dedicate a qh for the async ring head, since we couldn't unlink
	 * a 'real' qh without stopping the async schedule [4.8].  use it
	 * as the 'reclamation list head' too.
	 * its dummy is used in hw_alt_next of many tds, to prevent the qh
	 * from automatically advancing to the next td after short reads.
	 */
	ehci->async->hw_next = QH_NEXT(ehci->async->qh_dma);
	ehci->async->hw_info1 = cpu_to_hc32(QH_HEAD);
	ehci->async->hw_token = cpu_to_hc32(QTD_STS_HALT);
	ehci->async->hw_qtd_next = EHCI_LIST_END();
	ehci->async->hw_alt_next = EHCI_LIST_END();
	//QTD_NEXT(ehci->async->dummy->qtd_dma);
	ehci->ctrl_buffer =  USB_Alloc(sizeof(usbctrlrequest));
	ehci->command = 0;
	ehci_writel(0x00800002, &ehci->regs->command); 
	ehci_writel(ehci->periodic_dma, &ehci->regs->frame_list); 
	ehci_writel(ehci->async->qh_dma, &ehci->regs->async_next); 
	ehci_writel(0x00010009, &ehci->regs->command);
	ehci_writel(1, &ehci->regs->configured_flag);
	ehci_writel(0x00010029, &ehci->regs->command);

	return 0;
}

/* fill a qtd, returning how much of the buffer we were able to queue up */
static int qtd_fill(struct ehci_qtd *qtd, dma_addr_t buf, size_t len, int token, int maxpacket)
{
	int	i, count;
	u64	addr = buf;
	//ehci_dbg("fill qtd with dma %X len %X\n",buf,len);
	/* one buffer entry per 4K ... first might be short or unaligned */
	qtd->hw_buf[0] = cpu_to_hc32((u32)addr);
	qtd->hw_buf_hi[0] = 0;
	count = 0x1000 - (buf & 0x0fff);	/* rest of that page */
	if(likely(len < count))		/* ... iff needed */
	{
		count = len;
	}
	else 
	{
		buf +=  0x1000;
		buf &= ~0x0fff;

		/* per-qtd limit: from 16K to 20K (best alignment) */
		for(i = 1; count < len && i < 5; i++) 
		{
			addr = buf;
			qtd->hw_buf[i] = cpu_to_hc32((u32)addr);
			qtd->hw_buf_hi[i] = cpu_to_hc32((u32)(addr >> 32));
			buf += 0x1000;
			if((count + 0x1000) < len)
				count += 0x1000;
			else
				count = len;
		}

		/* short packets may only terminate transfers */
		if(count != len)
			count -= (count % maxpacket);
	}
	qtd->hw_token = cpu_to_hc32((count << 16) | token);
	qtd->length = count;

	return count;
}

// high bandwidth multiplier, as encoded in highspeed endpoint descriptors
#define hb_mult(wMaxPacketSize) (1 + (((wMaxPacketSize) >> 11) & 0x03))
// ... and packet size, for any kind of endpoint descriptor
#define max_packet(wMaxPacketSize) ((wMaxPacketSize) & 0x07ff)

/*
 * reverse of qh_urb_transaction:  free a list of TDs.
 * also count the actual transfer length.
 * 
 */
static void qh_end_transfer(void)
{
	struct ehci_qtd	*qtd;
	struct ehci_qh	*qh = ehci->asyncqh;
	u32 token;
	int error = 0;
	for(qtd = qh->qtd_head; qtd; qtd = qtd->next)
	{
		token = hc32_to_cpu(qtd->hw_token);
		if(likely(QTD_PID (token) != 2))
			qtd->urb->actual_length += qtd->length - QTD_LENGTH(token);			
				
		if(!(qtd->length == 0 && ((token & 0xff) == QTD_STS_HALT)) && (token & QTD_STS_HALT)) 
		{
#ifdef DEBUG
			ehci_dbg("EHCI:");
			ehci_dbg("qTD error!:");
			if (token & QTD_STS_BABBLE) 	/* FIXME "must" disable babbling device's port too */				
				ehci_dbg(" BABBLE"); 
			if (token & QTD_STS_MMF) 		/* fs/ls interrupt xfer missed the complete-split */
				ehci_dbg(" Missed micro frame");
			if (token & QTD_STS_DBE)
			{
				if(QTD_PID(token) == 1)
					ehci_dbg(" hc couldn't read data");
				else
					ehci_dbg(" hc couldn't write data");
			}
			if (token & QTD_STS_XACT)		/* timeout, bad CRC, wrong PID, etc */
				ehci_dbg(" Wrong ack");
			if (QTD_CERR(token) == 0)
				ehci_dbg(" Toomany errors");
						
			ehci_dbg(" token: %08x\n", token);
#endif
			error = -1;
		}
	}
	if(error)
	{
		dump_qh(ehci->asyncqh);
		qtd->urb->actual_length = error;
	}
	ehci->qtd_used = 0;
}

/*
 * create a list of filled qtds for this URB; won't link into qh.
 */
struct ehci_qtd *qh_urb_transaction (struct ehci_urb *urb) 
{
	struct ehci_qtd	*qtd, *qtd_prev;
	struct ehci_qtd	*head;
	dma_addr_t		buf;
	int len, maxpacket;
	int	is_input;
	u32	token;

	/*
	 * URBs map to sequences of QTDs:  one logical transaction
	 */
	head = qtd = ehci_qtd_alloc();
	if(!head)
		return NULL;
		
	qtd->urb = urb;

	urb->actual_length = 0;
	token = QTD_STS_ACTIVE;
	token |= (EHCI_TUNE_CERR << 10);
	/* for split transactions, SplitXState initialized to zero */

	len = urb->transfer_buffer_length;
	is_input = urb->input;
	if (urb->ep==0) /* is control */
	{
		/* SETUP pid */
		qtd_fill( qtd, urb->setup_dma, sizeof (usbctrlrequest), token | (2 /* "setup" */ << 8), 8);
		/* ... and always at least one more pid */
		token ^= QTD_TOGGLE;
		qtd_prev = qtd;
		qtd = ehci_qtd_alloc();
		if(!qtd)
			return NULL;
			
		qtd->urb = urb;
		qtd_prev->hw_next = QTD_NEXT(qtd->qtd_dma);
		qtd_prev->next = qtd;

		/* for zero length DATA stages, STATUS is always IN */
		if(len == 0)
			token |= (1 /* "in" */ << 8);
	}

	/*
	 * data transfer stage:  buffer setup
	 */
	buf = urb->transfer_dma;

	if(is_input)
		token |= (1 /* "in" */ << 8);
	/* else it's already initted to "out" pid (0 << 8) */

	maxpacket = max_packet(urb->maxpacket);

	/*
	 * buffer gets wrapped in one or more qtds;
	 * last one may be "short" (including zero len)
	 * and may serve as a control status ack
	 */
	for(;;) 
	{
		int this_qtd_len;

		this_qtd_len = qtd_fill(qtd, buf, len, token, maxpacket);
		len -= this_qtd_len;
		buf += this_qtd_len;

		/*
		 * short reads advance to a "magic" dummy instead of the next
		 * qtd ... that forces the queue to stop, for manual cleanup.
		 * (this will usually be overridden later.)
		 */
		if(is_input)
			qtd->hw_alt_next = ehci->asyncqh->hw_alt_next;

		/* qh makes control packets use qtd toggle; maybe switch it */
		if((maxpacket & (this_qtd_len + (maxpacket - 1))) == 0)
			token ^= QTD_TOGGLE;

		if(likely (len <= 0))
			break;

		qtd_prev = qtd;
		qtd = ehci_qtd_alloc();
		if(!qtd)
			return NULL;
			
		qtd->urb = urb;
		qtd_prev->hw_next = QTD_NEXT( qtd->qtd_dma);
		qtd_prev->next = qtd;
	}

	qtd->hw_alt_next = EHCI_LIST_END();

	/*
	 * control requests may need a terminating data "status" ack;
	 * bulk ones may need a terminating short packet (zero length).
	 */
	if(likely(urb->transfer_buffer_length != 0)) 
	{
		int	one_more = 0;

		if(urb->ep == 0) 
		{
			one_more = 1;
			token ^= 0x0100;	/* "in" <--> "out"  */
			token |= QTD_TOGGLE;	/* force DATA1 */
		} 
		//else if(!(urb->transfer_buffer_length % maxpacket)) 
		//{
			//one_more = 1;
		//}
		if(one_more) 
		{
			qtd_prev = qtd;
			qtd = ehci_qtd_alloc();
			if(!qtd)
				return NULL;
				
			qtd->urb = urb;
			qtd_prev->hw_next = QTD_NEXT( qtd->qtd_dma);
			qtd_prev->next = qtd;

			/* never any data in such packets */
			qtd_fill(qtd, 0, 0, token, 0);
		}
	}

	/* by default, enable interrupt on urb completion */
	qtd->hw_token |= cpu_to_hc32( QTD_IOC);
	return head;
}

int ehci_do_urb (struct ehci_device *dev, struct ehci_urb *urb)
{
	struct ehci_qh *qh;
	struct ehci_qtd *qtd;
	u32	info1 = 0, info2 = 0;
	//int	is_input;
	int	maxp = 0;
	int retval;

	//ehci_dbg ("do urb %X %X  ep %X\n",urb->setup_buffer,urb->transfer_buffer,urb->ep);
	if(urb->ep==0) //control message
		urb->setup_dma = ehci_dma_map_to(urb->setup_buffer,sizeof (usbctrlrequest));

	if(urb->transfer_buffer_length)
	{
		if(urb->input)
			urb->transfer_dma = ehci_dma_map_to(urb->transfer_buffer,urb->transfer_buffer_length);
		else
			urb->transfer_dma = ehci_dma_map_from(urb->transfer_buffer,urb->transfer_buffer_length);
	}
	qh = ehci->asyncqh;
	memset(qh, 0, 12 * 4);
	qtd = qh_urb_transaction(urb);
	if(!qtd) 
	{
		ehci->qtd_used=0;
		return -ENOMEM;
	}
	
	qh ->qtd_head = qtd;
		
	info1 |= ((urb->ep)&0xf)<<8;
	info1 |= dev->id;
	//is_input = urb->input;
	maxp = urb->maxpacket;
		
	info1 |= (2 << 12);	/* EPS "high" */
	if(urb->ep==0) // control
	{
		info1 |= (EHCI_TUNE_RL_HS << 28);
		info1 |= 64 << 16;	/* usb2 fixed maxpacket */
		info1 |= 1 << 14;	/* toggle from qtd */
		info2 |= (EHCI_TUNE_MULT_HS << 30);
	}
	else //bulk
	{
		info1 |= (EHCI_TUNE_RL_HS << 28);
		/* The USB spec says that high speed bulk endpoints
		 * always use 512 byte maxpacket.  But some device
		 * vendors decided to ignore that, and MSFT is happy
		 * to help them do so.  So now people expect to use
		 * such nonconformant devices with Linux too; sigh.
		 */
		info1 |= max_packet(maxp) << 16;
		info2 |= (EHCI_TUNE_MULT_HS << 30);				
	}
	//ehci_dbg("HW info: %08X\n",info1);
	qh->hw_info1 = cpu_to_hc32(info1);
	qh->hw_info2 = cpu_to_hc32(info2);

	qh->hw_next = QH_NEXT(qh->qh_dma);
	qh->hw_qtd_next = QTD_NEXT( qtd->qtd_dma);
	qh->hw_alt_next = EHCI_LIST_END();

	if(urb->ep!=0)
	{
		if(get_toggle(dev,urb->ep))
			qh->hw_token |= cpu_to_hc32(QTD_TOGGLE);
		else
			qh->hw_token &= ~cpu_to_hc32(QTD_TOGGLE);
			
		//ehci_dbg("toggle for ep %x: %d %x\n",urb->ep,get_toggle(dev,urb->ep),qh->hw_token);
	}

	qh->hw_token &= cpu_to_hc32( QTD_TOGGLE | QTD_STS_PING);
	qh->hw_next = QH_NEXT(ehci->async->qh_dma);

	ehci_dma_map_bidir(qh,sizeof(struct ehci_qh));
	for(qtd = qh->qtd_head; qtd; qtd = qtd->next)
		ehci_dma_map_bidir(qtd,sizeof(struct ehci_qtd));
//#if 0
//	if(urb->ep!=0)
//	{
//		dump_qh(ehci->async);
//		dump_qh(ehci->asyncqh);
//	}
//#endif   
	// start (link qh)
	ehci->async->hw_next = QH_NEXT(qh->qh_dma);
	ehci_dma_map_bidir(ehci->async,sizeof(struct ehci_qh));

	retval = handshake(&ehci->regs->port_status[dev->port], &ehci->regs->status, STS_INT, STS_INT, 1000*1000);

	//print_hex_dump_bytes ("qh mem",0,(void*)qh,17*4);
	//retval = poll_transfer_end(1000*1000);
	ehci_dma_unmap_bidir(ehci->async->qh_dma,sizeof(struct ehci_qh));
	ehci_dma_unmap_bidir(qh->qh_dma,sizeof(struct ehci_qh));
	for(qtd = qh->qtd_head; qtd; qtd = qtd->next)
		ehci_dma_unmap_bidir(qtd->qtd_dma,sizeof(struct ehci_qtd));

	// stop (unlink qh)
	ehci->async->hw_next = QH_NEXT(ehci->async->qh_dma);
	ehci_dma_map_bidir(ehci->async,sizeof(struct ehci_qh));
	ehci_dma_unmap_bidir(ehci->async->qh_dma,sizeof(struct ehci_qh));

	// ack
	ehci_writel( STS_RECL|STS_IAA|STS_INT, &ehci->regs->status);

	if(urb->ep!=0)
	{
		set_toggle(dev,urb->ep,(qh->hw_token & cpu_to_hc32(QTD_TOGGLE))?1:0);
		//ehci_dbg("toggle for ep %x: %d %d %x %X\n",urb->ep,get_toggle(dev,urb->ep),(qh->hw_token & cpu_to_hc32(QTD_TOGGLE)),qh->hw_token,dev->toggles);
	}

	if(retval>=0)
		// wait hc really stopped
		retval = handshake(&ehci->regs->port_status[dev->port], &ehci->regs->async_next,~0,ehci->async->qh_dma,1*1000);
		//release memory, and actualise urb->actual_length
	qh_end_transfer();


	if(urb->transfer_buffer_length)
	{
		if(urb->input)
			ehci_dma_unmap_to(urb->transfer_dma,urb->transfer_buffer_length);
		else
			ehci_dma_unmap_from(urb->transfer_dma,urb->transfer_buffer_length);
	}
	if(urb->ep==0) //control message
		ehci_dma_unmap_to(urb->setup_dma,sizeof (usbctrlrequest));
	
	if(retval==0)				
		return urb->actual_length;
#ifdef DEBUG
	ehci_dbg ("EHCI:Unsuccessfull urb %d!!\n", retval);
#endif
	return retval;
}

s32 ehci_control_message(struct ehci_device *dev,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *buf)
{
	struct ehci_urb urb;
	usbctrlrequest *req = ehci->ctrl_buffer;
#ifdef DEBUG
		ehci_dbg ("EHCI:Control msg: rt%02X r%02X v%04X i%04X s%04x %p\n", bmRequestType, bmRequest, wValue, wIndex,wLength,buf);
#endif
		
	req->bRequestType = bmRequestType;
	req->bRequest = bmRequest;
	req->wValue = swab16(wValue);
	req->wIndex = swab16(wIndex);
	req->wLength = swab16(wLength);
	urb.setup_buffer = req;
	urb.ep = 0;
	urb.input = (bmRequestType&USB_CTRLTYPE_DIR_DEVICE2HOST)!=0;
	urb.maxpacket = 64;
	urb.transfer_buffer_length = wLength;
	if (((u32)buf) > 0x13880000) // HW cannot access this buffer, we allow this for convenience
	{
		int ret;
		urb.transfer_buffer = USB_Alloc(wLength);
#ifdef DEBUG
		ehci_dbg("EHCI:Alloc another buffer %p %p\n",buf,urb.transfer_buffer);
#endif
		
		memcpy(urb.transfer_buffer,buf,wLength);
		ret =  ehci_do_urb(dev,&urb);
		memcpy(buf,urb.transfer_buffer,wLength);
		USB_Free(urb.transfer_buffer);
		return ret;
	}
	else
	{
		urb.transfer_buffer = buf;
		return ehci_do_urb(dev,&urb);
	}
}

s32 ehci_bulk_message(struct ehci_device *dev,u8 bEndpoint,u16 wLength,void *rpData)
{
	struct ehci_urb urb;
	s32 ret;
	urb.setup_buffer = NULL;
	urb.ep = bEndpoint;
	urb.input = (bEndpoint&0x80)!=0;
	urb.maxpacket = 512;
	urb.transfer_buffer_length = wLength;
	urb.transfer_buffer = rpData;
#ifdef DEBUG
		ehci_dbg ("EHCI:Bulk msg: ep:%02X size:%02X addr:%04X", bEndpoint, wLength, rpData);
#endif
		
	ret= ehci_do_urb(dev,&urb);
#ifdef DEBUG
		ehci_dbg ( "==>%d\n", ret);
#endif

	return ret;
}

int ehci_reset_port_old(int port)
{
	u32 __iomem	*status_reg = &ehci->regs->port_status[port];
	struct ehci_device *dev = &ehci->devices[port];
	u32 status = ehci_readl(status_reg);
	int retval = 0, i, f;	
	dev->id = 0;
	
	if((PORT_OWNER&status) || !(PORT_CONNECT&status))
	{
		int retries = 10;
		while (!(PORT_CONNECT&status) && retries > 0)
		{
			msleep(1000);
			status = ehci_readl(status_reg);
#ifdef DEBUG
			ehci_dbg ("EHCI:Port %d status at retry %d %X \n", port, retries, status);
#endif
			retries--;
		}
		if( retries <= 0 )
		{		
			ehci_writel( PORT_OWNER, status_reg);
#ifdef DEBUG
			ehci_dbg ("EHCI:Port %d had no usb2 device connected at startup %X\n", port, ehci_readl(status_reg));
#endif
			return -ENODEV;// no USB2 device connected
		}
	}
#ifdef DEBUG
	ehci_dbg ("EHCI:Port %d has usb2 device connected! Reset it...\n", port);
#endif
	
	f = handshake_mode;
	handshake_mode = 1;
	
	for(i = 0; i < 4; i++)  //4 retries
	{ 
		u32 status = ehci_readl(status_reg);
		status &= ~PORT_PE;
		status |= PORT_RESET | PORT_POWER;       
        ehci_writel( status,status_reg);
        msleep(100);// wait 60ms for the reset sequence
        status=ehci_readl(status_reg);
        status &= ~(PORT_RWC_BITS | PORT_RESET); /* force reset to complete */
		ehci_writel( status,status_reg);
		msleep(100);
		status = ehci_readl(status_reg);
		if((PORT_OWNER&status) || !(PORT_CONNECT&status) || !(status & PORT_PE) || !(status & PORT_POWER))
		{
			retval=-1;
			continue;
		}
		
		retval = handshake(status_reg, status_reg, PORT_RESET, 0, 750);
		if (retval == 0) 
		{               
#ifdef DEBUG	
			ehci_dbg ( "EHCI:Port %d reseted status: %04x...\n", port,ehci_readl(status_reg));
#endif
			msleep(100);
			retval = ehci_control_message(dev, USB_CTRLTYPE_DIR_DEVICE2HOST, USB_REQ_GETDESCRIPTOR, USB_DT_DEVICE << 8, 0, sizeof(dev->desc), &dev->desc);
        
			if(retval >= 0) 
				retval = ehci_control_message(dev, USB_CTRLTYPE_DIR_HOST2DEVICE, USB_REQ_SETADDRESS, port + 1, 0, 0, 0);
				
			if(retval>=0)
				break;
		}
	}
	
	handshake_mode = f;
	
	if(retval < 0)
		return retval;
	
	dev->toggles = 0;

	dev->id = port + 1;
	
#ifdef DEBUG
	ehci_dbg ("EHCI:Device %d: %X %X...\n", dev->id,le16_to_cpu(dev->desc.idVendor), le16_to_cpu(dev->desc.idProduct));
#endif
		
	int relport = port == 0 ? 1 : 0;
	
	ehci_release_port(relport);
	
	return retval;
}

void ehci_adquire_port(int port)
{
	u32 __iomem	*status_reg = &ehci->regs->port_status[port];
	u32 status = ehci_readl(status_reg); 

	//change owner, port disabled
	status ^= PORT_OWNER;
	status &= ~(PORT_PE | PORT_RWC_BITS);
	ehci_writel(status, status_reg);	
	msleep(5);
	status = ehci_readl(status_reg);
	status ^= PORT_OWNER;
	status &= ~(PORT_PE | PORT_RWC_BITS);
	ehci_writel(status, status_reg);	
	msleep(5);	

	//enable port	
	ehci_writel(0x1001, status_reg);
    msleep(5);
}

int ehci_reset_usb_port(int port)
{
    u32 __iomem *status_reg = &ehci->regs->port_status[port];
    u32 status = ehci_readl(status_reg);
   
    int i, f, retval = 0;
   
    if((PORT_OWNER&status) || !(PORT_CONNECT&status))
    {
		ehci_writel( PORT_OWNER,status_reg);
		return -ENODEV;// no USB2 device connected
    }
       
	f = handshake_mode;
    handshake_mode = 1;

    for(i = 0; i < 4; i++)  //4 retries
    {       
		status &= ~PORT_PE;
		status |= PORT_RESET;       
        ehci_writel( status,status_reg);
        msleep(60);// wait 60ms for the reset sequence
        status = ehci_readl(status_reg);
        status &= ~(PORT_RWC_BITS | PORT_RESET); /* force reset to complete */
		ehci_writel( status,status_reg);
        msleep(50);                     
        retval = handshake(status_reg, status_reg, PORT_RESET, 0, 750);
        if(retval != 0) 
		{
			status=ehci_readl(status_reg);
			handshake_mode=f;
			return -2000;
        }
        status=ehci_readl(status_reg);
		if(status & PORT_PE) 
			break; //port enabled
	}
	
	handshake_mode = f; 
   
	if(!(status & PORT_PE)) 
	{
		// that means is low speed device so release
		status |= PORT_OWNER;
		status &= ~PORT_RWC_BITS;
		ehci_writel( status, status_reg);      
		msleep(10);
		status = ehci_readl(status_reg);   
	}
	
	int relport = port == 0 ? 1 : 0;
	
	ehci_release_port(relport);
	return retval;   
}

int ehci_init_port(int port)
{	
	struct ehci_device *dev = &ehci->devices[port];
	int retval = 0;
	dev->id = 0;
	int i;
	msleep(50);
	for(i = 0; i < 3; i++)
	{
        retval = ehci_control_message(dev, USB_CTRLTYPE_DIR_DEVICE2HOST, USB_REQ_GETDESCRIPTOR, USB_DT_DEVICE << 8, 0,sizeof(dev->desc), &dev->desc);
        if(retval < 0) 
		{
			retval=-2201;
			msleep(100);

        }
        else 
			break;
    }
    if(retval < 0)
    {
		for(i = 0; i < 3; i++)
		{
	        ehci_reset_usb_port(port);
	        msleep(100);
	        retval = ehci_control_message(dev, USB_CTRLTYPE_DIR_DEVICE2HOST, USB_REQ_GETDESCRIPTOR, USB_DT_DEVICE << 8, 0, sizeof(dev->desc), &dev->desc);
	        if(retval < 0)
				retval=-2201;
	        else 
				break;
	    }
    }
    
    if(retval < 0)
    {
		for(i = 0; i < 3; i++)
		{
			ehci_adquire_port(port);
	        msleep(100);
	        ehci_reset_usb_port(port);
	        msleep(100);

	        retval = ehci_control_message(dev, USB_CTRLTYPE_DIR_DEVICE2HOST, USB_REQ_GETDESCRIPTOR, USB_DT_DEVICE << 8, 0, sizeof(dev->desc), &dev->desc);
	        if(retval < 0)
				retval=-2201;
	        else 
				break;
	    }
    }

    if(retval < 0) 	
		return -2201;

	int cnt=2;
	do{
		msleep(100);

		retval = ehci_control_message(dev, USB_CTRLTYPE_DIR_HOST2DEVICE, USB_REQ_SETADDRESS, cnt, 0, 0, 0);
		if(retval < 0)                
			retval=-8000-cnt;
	
		dev->toggles = 0;	
		dev->id = cnt;	
		USB_ClearHalt(dev, 0);
		msleep(100);

		retval = ehci_control_message(dev, USB_CTRLTYPE_DIR_DEVICE2HOST, USB_REQ_GETDESCRIPTOR, USB_DT_DEVICE << 8, 0, sizeof(dev->desc), &dev->desc);
	                             
		if(retval < 0)
		{
			retval = -2242;
			dev->id = 0;
		}
        
	}while(retval < 0 && cnt < 20);
        

	return retval;
}

int ehci_reset_port(int port)
{
	int retval;
	retval = ehci_reset_usb_port(port);
	if(retval >= 0)
		retval = ehci_init_port(port);
		
	return retval;
}

int ehci_reset_port2(int port)
{
	u32 __iomem	*status_reg = &ehci->regs->port_status[port];
	int ret=ehci_reset_port_old(port);
	if(ret < 0/*==-ENODEV || ret==-ETIMEDOUT*/)
	{
		u32 status=ehci_readl(status_reg);
		if(status &  1)
		{
			msleep(500); // power off 
			ehci_writel( 0x1001 ,status_reg); // power on
		}
	}
	return ret;
}

int ehci_reset_device(struct ehci_device *dev)
{
	return ehci_reset_port(dev->port);
}

#include "usbstorage.h"
int ehci_discover(void)
{
	int i;
	int ret = 0;
	// precondition: the ehci should be halted
	for(i = 0; i < ehci->num_port; i++)
	{
		struct ehci_device *dev = &ehci->devices[i];
		dev->port = i;
		ret = ehci_reset_port(i);
		if( ret != -ENODEV )
			break;
	}        
	return ret;
}
/* wii: quickly release non ehci or not connected ports,
   as we can't kick OHCI drivers laters if we discover a device for them.
*/
int ehci_release_ports(void)
{
	int i;
	u32 __iomem	*status_reg = &ehci->regs->port_status[2];
	while(ehci_readl(&ehci->regs->port_status[2]) == 0x1000);// wait port 2 to init
	msleep(1);// wait another msec..
	for(i = 0;i<ehci->num_port; i++)
	{
		status_reg = &ehci->regs->port_status[i];
		u32 status = ehci_readl(status_reg);
		if (/*i==1 ||*/ i == 2 || !(PORT_CONNECT&status) || PORT_USB11(status))
			ehci_writel(PORT_OWNER,status_reg); // release port.

	}
	return 0;
}

int ehci_release_port(int port)
{
	u32 __iomem	*status_reg = &ehci->regs->port_status[2];
	while(ehci_readl(&ehci->regs->port_status[2]) == 0x1000);
	msleep(1);
	status_reg = &ehci->regs->port_status[port];		
	ehci_writel(PORT_OWNER, status_reg);

	return 0;
}

int ehci_open_device(int vid,int pid,int fd)
{
	int i;
	for(i=0;i<ehci->num_port;i++)
	{
		//ehci_dbg("try device: %d\n",i);
		if(ehci->devices[i].fd == 0 && le16_to_cpu(ehci->devices[i].desc.idVendor) == vid && le16_to_cpu(ehci->devices[i].desc.idProduct) == pid)
		{
			//ehci_dbg("found device: %x %x\n",vid,pid);
			ehci->devices[i].fd = fd;
			return fd;
		}
	}
	return -6;
}

int ehci_close_device(struct ehci_device *dev)
{
	if (dev)
		dev->fd = 0;
		
	return 0;
}
 void * ehci_fd_to_dev(int fd)
{
	int i;
	for(i = 0;i < ehci->num_port; i++)
	{
		struct ehci_device *dev = &ehci->devices[i];
		//ehci_dbg ( "device %d:fd:%d %X %X...\n", dev->id,dev->fd,le16_to_cpu(dev->desc.idVendor),le16_to_cpu(dev->desc.idProduct));
		if(dev->fd == fd)
			return dev;
	}
#ifdef DEBUG
	ehci_dbg("EHCI:Unkown fd! %d\n",fd);
#endif
	return 0;
}

#define g_ehci #error
int ehci_get_device_list(u8 maxdev, u8 b0, u8 *num, u16 *buf)
{
	int i, j = 0;
	for(i = 0; i < ehci->num_port && j < maxdev; i++)
	{
		struct ehci_device *dev = &ehci->devices[i];
		if(dev->id != 0)
		{
#ifdef DEBUG
			ehci_dbg ( "EHCI:Device %d: %X %X...\n", dev->id,le16_to_cpu(dev->desc.idVendor),le16_to_cpu(dev->desc.idProduct));
#endif
			buf[j*4] = 0;
			buf[j*4+1] = 0;
			buf[j*4+2] = le16_to_cpu(dev->desc.idVendor);
			buf[j*4+3] = le16_to_cpu(dev->desc.idProduct);
			j++;
		}
	}
#ifdef DEBUG
	ehci_dbg("EHCI:Found %d devices\n",j);
#endif
	*num = j;
	return 0;
}

#include "usb.c"
#include "usbstorage.c"
