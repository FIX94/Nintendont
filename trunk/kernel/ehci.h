/*
 * Copyright (c) 2009 Kwiirk
 * Original Copyright (c) 2001-2002 by David Brownell
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

#include "ehci_types.h"

#ifndef __LINUX_EHCI_HCD_H
#define __LINUX_EHCI_HCD_H
/* definitions used for the EHCI driver */

/*
 * __hc32 and __hc16 are "Host Controller" types, they may be equivalent to
 * __leXX (normally) or __beXX (given EHCI_BIG_ENDIAN_DESC), depending on
 * the host controller implementation.
 *
 * To facilitate the strongest possible byte-order checking from "sparse"
 * and so on, we use __leXX unless that's not practical.
 */
#ifdef CONFIG_USB_EHCI_BIG_ENDIAN_DESC
typedef __u32 __bitwise __hc32;
typedef __u16 __bitwise __hc16;
#else
#define __hc32	__le32
#define __hc16	__le16
#endif


#define	EHCI_MAX_ROOT_PORTS	4		/* see HCS_N_PORTS */
#define EHCI_MAX_QTD		8
#include "usb.h"

struct ehci_device{
        usb_devdescr desc;
        int id;
        int port;
        int fd;
        u32 toggles;
};
#define ep_bit(ep) (((ep)&0xf)+(((ep)>>7)?16:0))
#define get_toggle(dev,ep) (((dev)->toggles>>ep_bit(ep))&1)
#define set_toggle(dev,ep,v) (dev)->toggles = ((dev)->toggles &(~(1<<ep_bit(ep)))) | ((v)<<ep_bit(ep))

struct ehci_urb{
        void* setup_buffer;
        dma_addr_t setup_dma;

        void* transfer_buffer;
        dma_addr_t transfer_dma;
        u32 transfer_buffer_length;
        u32 actual_length;

        u8 ep;
        u8 input;
        u32 maxpacket;
};
struct ehci_hcd {			/* one per controller */
	/* glue to PCI and HCD framework */
	void __iomem *_regs;
	struct ehci_caps __iomem *caps;
	struct ehci_regs __iomem *regs;
	struct ehci_dbg_port __iomem *debug;
        void		        *device;
	__u32			hcs_params;	/* cached register copy */

	/* async schedule support */
	struct ehci_qh		*async; // the head never gets a qtd inside.
	struct ehci_qh		*asyncqh;
        
        struct ehci_qtd	        *qtds[EHCI_MAX_QTD];
        int		        qtd_used;
	unsigned long		next_statechange;
	u32			command;
        
        /* HW need periodic table initialised even if we dont use it @todo:is it really true? */
#define	DEFAULT_I_TDPS		1024		/* some HCs can do less */
	__hc32			*periodic;	/* hw periodic table */
	dma_addr_t		periodic_dma;

        u8			num_port;
	struct ehci_device	devices[EHCI_MAX_ROOT_PORTS];	/* the attached device list per port */
        void *ctrl_buffer;		/* pre allocated buffer for control messages */
};
/*-------------------------------------------------------------------------*/

#include "ehci_defs.h"

/*-------------------------------------------------------------------------*/
#define	QTD_NEXT( dma)	cpu_to_hc32( (u32)dma)

/*
 * EHCI Specification 0.95 Section 3.5
 * QTD: describe data transfer components (buffer, direction, ...)
 * See Fig 3-6 "Queue Element Transfer Descriptor Block Diagram".
 *
 * These are associated only with "QH" (Queue Head) structures,
 * used with control, bulk, and interrupt transfers.
 */
struct ehci_qtd {
	/* first part defined by EHCI spec */
	__hc32			hw_next;	/* see EHCI 3.5.1 */
	__hc32			hw_alt_next;    /* see EHCI 3.5.2 */
	__hc32			hw_token;       /* see EHCI 3.5.3 */
#define	QTD_TOGGLE	(1 << 31)	/* data toggle */
#define	QTD_LENGTH(tok)	(((tok)>>16) & 0x7fff)
#define	QTD_IOC		(1 << 15)	/* interrupt on complete */
#define	QTD_CERR(tok)	(((tok)>>10) & 0x3)
#define	QTD_PID(tok)	(((tok)>>8) & 0x3)
#define	QTD_STS_ACTIVE	(1 << 7)	/* HC may execute this */
#define	QTD_STS_HALT	(1 << 6)	/* halted on error */
#define	QTD_STS_DBE	(1 << 5)	/* data buffer error (in HC) */
#define	QTD_STS_BABBLE	(1 << 4)	/* device was babbling (qtd halted) */
#define	QTD_STS_XACT	(1 << 3)	/* device gave illegal response */
#define	QTD_STS_MMF	(1 << 2)	/* incomplete split transaction */
#define	QTD_STS_STS	(1 << 1)	/* split transaction state */
#define	QTD_STS_PING	(1 << 0)	/* issue PING? */

#define ACTIVE_BIT(ehci)	cpu_to_hc32( QTD_STS_ACTIVE)
#define HALT_BIT(ehci)		cpu_to_hc32( QTD_STS_HALT)
#define STATUS_BIT(ehci)	cpu_to_hc32( QTD_STS_STS)

	__hc32			hw_buf [5];        /* see EHCI 3.5.4 */
	__hc32			hw_buf_hi [5];        /* Appendix B */

	/* the rest is HCD-private */
	dma_addr_t		qtd_dma;		/* qtd address */
	struct ehci_qtd		*next;			/* sw qtd list */
	struct ehci_urb		*urb;			/* qtd's urb */
	size_t			length;			/* length of buffer */
} __attribute__ ((aligned (32)));

/* mask NakCnt+T in qh->hw_alt_next */
#define QTD_MASK(ehci)	cpu_to_hc32 ( ~0x1f)

#define IS_SHORT_READ(token) (QTD_LENGTH (token) != 0 && QTD_PID (token) == 1)

/*-------------------------------------------------------------------------*/

/* type tag from {qh,itd,sitd,fstn}->hw_next */
#define Q_NEXT_TYPE(dma)	((dma) & cpu_to_hc32( 3 << 1))

/*
 * Now the following defines are not converted using the
 * __constant_cpu_to_le32() macro anymore, since we have to support
 * "dynamic" switching between be and le support, so that the driver
 * can be used on one system with SoC EHCI controller using big-endian
 * descriptors as well as a normal little-endian PCI EHCI controller.
 */
/* values for that type tag */
#define Q_TYPE_ITD	(0 << 1)
#define Q_TYPE_QH	(1 << 1)
#define Q_TYPE_SITD	(2 << 1)
#define Q_TYPE_FSTN	(3 << 1)

/* next async queue entry, or pointer to interrupt/periodic QH */
#define QH_NEXT(dma)	(cpu_to_hc32( (((u32)dma)&~0x01f)|Q_TYPE_QH))

/* for periodic/async schedules and qtd lists, mark end of list */
#define EHCI_LIST_END()	cpu_to_hc32( 1) /* "null pointer" to hw */

/*
 * Entries in periodic shadow table are pointers to one of four kinds
 * of data structure.  That's dictated by the hardware; a type tag is
 * encoded in the low bits of the hardware's periodic schedule.  Use
 * Q_NEXT_TYPE to get the tag.
 *
 * For entries in the async schedule, the type tag always says "qh".
 */
union ehci_shadow {
	struct ehci_qh		*qh;		/* Q_TYPE_QH */
	struct ehci_itd		*itd;		/* Q_TYPE_ITD */
	struct ehci_sitd	*sitd;		/* Q_TYPE_SITD */
	struct ehci_fstn	*fstn;		/* Q_TYPE_FSTN */
	__hc32			*hw_next;	/* (all types) */
	void			*ptr;
};

/*-------------------------------------------------------------------------*/

/*
 * EHCI Specification 0.95 Section 3.6
 * QH: describes control/bulk/interrupt endpoints
 * See Fig 3-7 "Queue Head Structure Layout".
 *
 * These appear in both the async and (for interrupt) periodic schedules.
 */

struct ehci_qh {
	/* first part defined by EHCI spec */
	__hc32			hw_next;	/* see EHCI 3.6.1 */
	__hc32			hw_info1;       /* see EHCI 3.6.2 */
#define	QH_HEAD		0x00008000
	__hc32			hw_info2;        /* see EHCI 3.6.2 */
#define	QH_SMASK	0x000000ff
#define	QH_CMASK	0x0000ff00
#define	QH_HUBADDR	0x007f0000
#define	QH_HUBPORT	0x3f800000
#define	QH_MULT		0xc0000000
	__hc32			hw_current;	/* qtd list - see EHCI 3.6.4 */

	/* qtd overlay (hardware parts of a struct ehci_qtd) */
	__hc32			hw_qtd_next;
	__hc32			hw_alt_next;
	__hc32			hw_token;
	__hc32			hw_buf [5];
	__hc32			hw_buf_hi [5];

	/* the rest is HCD-private */
	dma_addr_t		qh_dma;		/* address of qh */
	struct ehci_qtd		*qtd_head;	/* sw qtd list */

	struct ehci_hcd		*ehci;

#define NO_FRAME ((unsigned short)~0)			/* pick new start */
} __attribute__ ((aligned (32)));

/*-------------------------------------------------------------------------*/



/*-------------------------------------------------------------------------*/

/* cpu to ehci */
#define cpu_to_hc32(b) cpu_to_le32(b)
#define hc32_to_cpu(b) le32_to_cpu(b)
#define hc32_to_cpup(b) le32_to_cpu(*(b))

/*-------------------------------------------------------------------------*/

/* os specific functions */
void*ehci_maligned(int size,int alignement,int crossing);
dma_addr_t ehci_virt_to_dma(void *);
dma_addr_t ehci_dma_map_to(void *buf,size_t len);
dma_addr_t ehci_dma_map_from(void *buf,size_t len);
dma_addr_t ehci_dma_map_bidir(void *buf,size_t len);
void ehci_dma_unmap_to(dma_addr_t buf,size_t len);
void ehci_dma_unmap_from(dma_addr_t buf,size_t len);
void ehci_dma_unmap_bidir(dma_addr_t buf,size_t len);


/* extern API */

s32 ehci_control_message(struct ehci_device *dev,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *buf);
s32 ehci_bulk_message(struct ehci_device *dev,u8 bEndpoint,u16 wLength,void *rpData);
int ehci_discover(void);
int ehci_get_device_list(u8 maxdev,u8 b0,u8*num,u16*buf);

extern struct ehci_hcd *ehci; /* @todo put ehci as a static global and remove ehci from APIs.. */
extern int ehci_open_device(int vid,int pid,int fd);
extern int ehci_close_device(struct ehci_device *dev);
extern void * ehci_fd_to_dev(int fd);
extern int ehci_release_ports(void);
extern int ehci_release_port(int port);

/* UMS API */

s32 USBStorage_Init(void);
s32 USBStorage_Get_Capacity(u32*sector_size);
s32 USBStorage_Read_Sectors(u32 sector, u32 numSectors, void *buffer);
s32 USBStorage_Read_Stress(u32 sector, u32 numSectors, void *buffer);
s32 USBStorage_Write_Sectors(u32 sector, u32 numSectors, const void *buffer);

#ifndef DEBUG
#define STUB_DEBUG_FILES
#endif	/* DEBUG */

/*-------------------------------------------------------------------------*/

#endif /* __LINUX_EHCI_HCD_H */
