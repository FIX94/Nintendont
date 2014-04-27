#ifndef __EXI_H_
/* Retrieve the I/O register base for a given EXI channel */
#define EXI_ADDR(channel) (0xCD006800 + (channel) * 0x14)
#define EXI_CSR 0x0000	/* EXI Channel 0 Parameter Register */
#define EXI_CSR_ROMDIS	(1 << 13)	/* EXI0: de-scramble logic disabled */
#define EXI_CSR_EXT		(1 << 12)		/* Device connected if 1 */
#define EXI_CSR_EXTINT	(1 << 11)	/* External Insertion Interrupt Status */
#define EXI_CSR_EXTINTMASK	(1 << 10)	/* EXT interrupt mask */
#define EXI_CSR_CLK_1MHZ	(0 << 4)	/* Clock: 1MHz */
#define EXI_CSR_CLK_2MHZ	(1 << 4)	/* Clock: 2MHz */
#define EXI_CSR_CLK_4MHZ	(2 << 4)	/* Clock: 4MHz */
#define EXI_CSR_CLK_8MHZ	(3 << 4)	/* Clock: 8MHz */
#define EXI_CSR_CLK_16MHZ	(4 << 4)	/* Clock: 16MHz */
#define EXI_CSR_CLK_32MHZ	(5 << 4)	/* Clock: 32MHz */
#define EXI_CSR_CS(x)	((x) << 7)	/* Chip select */
#define EXI_CSR_TCINT	(1 << 3)		/* Transfer complete interrupt status */
#define EXI_CSR_TCINTMASK	(1 << 2)	/* Transfer complete interrupt mask */
#define EXI_CSR_EXTSTAT	(1 << 1)	/* Interrupt status */
#define EXI_CSR_EXTSTATMASK	(1 << 0)	/* EXI interrupt mask */
#define EXI_CR 0x0000c	/* EXI Channel 0 Control Register */
#define EXI_CR_TLEN_1	(0 << 4)	/* Transfer length: 1 byte */
#define EXI_CR_TLEN_2	(1 << 4)	/* Transfer length: 2 byte */
#define EXI_CR_TLEN_3	(2 << 4)	/* Transfer length: 3 byte */
#define EXI_CR_TLEN_4	(4 << 4)	/* Transfer length: 4 byte */
#define EXI_CR_RW_R		(0 << 2)	/* Transfer type: read */
#define EXI_CR_RW_W		(1 << 2)	/* Transfer type: write */
#define EXI_CR_RW_RW	(2 << 2)	/* Transfer type: read/write */
#define EXI_CR_DMA	(1 << 1)	/* DMA transfer mode */
#define EXI_CR_TSTART	(1 << 0)	/* Start transfer */
#define EXI_DATA 0x0010	/* EXI Channel 0 Immediate Data */

#endif

