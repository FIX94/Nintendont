#ifndef __HW_H__
#define __HW_H__

#define MSR_POW		(1<<18)
#define MSR_ILE		(1<<16)
#define MSR_EE		(1<<15)
#define MSR_PR		(1<<14)
#define MSR_FP		(1<<13)
#define MSR_ME		(1<<12)
#define MSR_FE0		(1<<11)
#define MSR_SE		(1<<10)
#define MSR_BE		(1<< 9)
#define MSR_FE1		(1<< 8)
#define MSR_IP		(1<< 6)
#define MSR_IR		(1<< 5)
#define MSR_DR		(1<< 4)
#define MSR_RI		(1<< 1)
#define MSR_LE		(1<< 0)

#define HID0_EMCP	(1<<31)
#define HID0_DBP	(1<<30)
#define HID0_EBA	(1<<29)
#define HID0_EBD	(1<<28)
#define HID0_BCLK	(1<<27)
#define HID0_ECLK	(1<<25)
#define HID0_PAR	(1<<24)
#define HID0_DOZE	(1<<23)
#define HID0_NAP	(1<<22)
#define HID0_SLEEP	(1<<21)
#define HID0_DPM	(1<<20)
#define HID0_NHR	(1<<16)
#define HID0_ICE	(1<<15)
#define HID0_DCE	(1<<14)
#define HID0_ILOCK	(1<<13)
#define HID0_DLOCK	(1<<12)
#define HID0_ICFI	(1<<11)
#define HID0_DCFI	(1<<10)
#define HID0_SPD	(1<< 9)
#define HID0_IFEM	(1<< 8)
#define HID0_SGE	(1<< 7)
#define HID0_DCFA	(1<< 6)
#define HID0_BTIC	(1<< 5)
#define HID0_ABE	(1<< 3)
#define HID0_BHT	(1<< 2)
#define HID0_NOOPTI	(1<< 0)

#define HID2_LSQE	(1<<31)
#define HID2_WPE	(1<<30)
#define HID2_PSE	(1<<29)
#define HID2_LCE	(1<<28)

#define L2CR_L2E	(1<<31)
#define L2CR_L2CE	(1<<30)
#define L2CR_L2DO	(1<<22)
#define L2CR_L2I	(1<<21)
#define L2CR_L2WT	(1<<19)
#define L2CR_L2TS	(1<<18)
#define L2CR_L2IP	(1<< 0)

#define DMAU_MEM_ADDR_MASK	0xFFFFFFE0
#define DMAU_LENU(x)		(x & 0x1F)
#define DMAL_LC_ADDR_MASK	0xFFFFFFE0
#define DMAL_LD				(1<< 4)
#define DMAL_LENL(x)		(x & 0xC)
#define DMAL_T				(1<< 1)
#define DMAL_F				(1<< 0)

#define BATU_BEPI_MASK		0xFFFC0000
#define BATU_BL(x)			(x & 0x00001FFC)
#define BATU_VS				(1<< 1)
#define BATU_VP				(1<< 0)
#define BATL_BRPN_MASK		0xFFFC0000
#define BATL_WIMG_MASK		0x78
#define BATL_PP				(1<< 0)

// BATU - 0x80001FFF	== 256Mbytes
// 1000 0000 000x xxx0 0001 1111 1111 11xx
// 0x80000000|256Mbytes|VS|VP
// BATL - 0x00000002
// 0000 0000 0000 000x xxxx xxxx x000 0x10
// PP=b10
//
// BATU - 0xC0001FFF	== 256Mbytes
// BATL - 0x0000002a
// 0000 0000 0000 000x xxxx xxxx x010 1x10
// WIMG=b0101|PP=b10
//

#define rHID2	920
#define rDMAU	922
#define rDMAL	923
#define rHID0	1008
#define rHID1	1009
#define rHID4	1011

/*
 * Upper PTE
 * 0|1-24|25|26-31
 * V|VSID|H |API
 *
 * Lower PTE
 * 0-19|20-22|23|24|25-28|29|30-31
 * RPN |000  |R |C |WIMG |0 |PP
*/

#endif

