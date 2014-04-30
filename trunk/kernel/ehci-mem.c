/*
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

/* this file is part of ehci.c */


static inline void ehci_qtd_init(struct ehci_qtd *qtd)
{
	dma_addr_t dma = ehci_virt_to_dma(qtd);
	memset (qtd, 0, sizeof *qtd);
	qtd->qtd_dma = dma;
	qtd->hw_token = (QTD_STS_HALT);
	qtd->hw_next = EHCI_LIST_END();
	qtd->hw_alt_next = EHCI_LIST_END();
}

static inline struct ehci_qtd * ehci_qtd_alloc(void)
{
	struct ehci_qtd *qtd ;
	if(ehci->qtd_used>=EHCI_MAX_QTD)
		return NULL;
		
	qtd = ehci->qtds[ehci->qtd_used++];
	ehci_qtd_init(qtd);
	return qtd;
}

int ehci_mem_init (void)
{
	int i;
#if 1
	ehci->periodic = ehci_maligned(DEFAULT_I_TDPS * sizeof(__le32), 32, 4096);
	ehci->periodic_dma = ehci_virt_to_dma(ehci->periodic);

	for(i = 0; i < DEFAULT_I_TDPS; i++)
		ehci->periodic[i] = EHCI_LIST_END();
	
	ehci_writel(ehci->periodic_dma, &ehci->regs->frame_list);
#else
	dbgprintf("ehci periodic:%x\n", ehci_readl(ehci,  &ehci->regs->frame_list));
	dbgprintf("ehci *periodic:%x\n", *(u32*)ehci_readl(ehci, &ehci->regs->frame_list));
#endif
	for(i = 0; i < EHCI_MAX_QTD; i++)
		ehci->qtds[i] = ehci_maligned(sizeof(struct ehci_qtd), 32, 4096);		
        
	ehci->qtd_used = 0;
	ehci->asyncqh = ehci_maligned(sizeof(struct ehci_qh), 32, 4096);
	ehci->asyncqh->ehci = ehci;
	ehci->asyncqh->qh_dma = ehci_virt_to_dma(ehci->asyncqh);
	ehci->asyncqh->qtd_head = NULL;
	ehci->async = ehci_maligned(sizeof(struct ehci_qh), 32, 4096);
	ehci->async->ehci = ehci;
	ehci->async->qh_dma = ehci_virt_to_dma(ehci->async);
	ehci->async->qtd_head = NULL;

	return 0;
}
