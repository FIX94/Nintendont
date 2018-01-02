/*---------------------------------------------------------------------------------------------
 * USB Gecko Development Kit - http://www.usbgecko.com
 * --------------------------------------------------------------------------------------------
 * 
 *
 * usb.h - functions for the USB Gecko adapter (www.usbgecko.com).
 *
 * Copyright (c) 2008 - Nuke - <wiinuke@gmail.com>
 * 
 *---------------------------------------------------------------------------------------------*/

#ifndef __USB_H__
#define __USB_H__

#define exi_chan0sr			*(volatile unsigned int*) 0xCD006800 // Channel 0 Status Register
#define exi_chan1sr			*(volatile unsigned int*) 0xCD006814 // Channel 1 Status Register
#define exi_chan2sr			*(volatile unsigned int*) 0xCD006828 // Channel 2 Status Register
#define exi_chan0cr			*(volatile unsigned int*) 0xCD00680c // Channel 0 Control Register
#define exi_chan1cr			*(volatile unsigned int*) 0xCD006820 // Channel 1 Control Register
#define exi_chan2cr			*(volatile unsigned int*) 0xCD006834 // Channel 2 Control Register
#define exi_chan0data		*(volatile unsigned int*) 0xCD006810 // Channel 0 Immediate Data
#define exi_chan1data		*(volatile unsigned int*) 0xCD006824 // Channel 1 Immediate Data
#define exi_chan2data		*(volatile unsigned int*) 0xCD006838 // Channel 2 Immediate Data
#define exi_chan0dmasta		*(volatile unsigned int*) 0xCD006804 // Channel 0 DMA Start address
#define exi_chan1dmasta		*(volatile unsigned int*) 0xCD006818 // Channel 1 DMA Start address
#define exi_chan2dmasta		*(volatile unsigned int*) 0xCD00682c // Channel 2 DMA Start address
#define exi_chan0dmalen		*(volatile unsigned int*) 0xCD006808 // Channel 0 DMA Length
#define exi_chan1dmalen		*(volatile unsigned int*) 0xCD00681c // Channel 1 DMA Length
#define exi_chan2dmalen		*(volatile unsigned int*) 0xCD006830 // Channel 2 DMA Length

#ifdef DEBUG
// Function prototypes
void usb_flush();
int usb_checkgecko();
void usb_sendbuffer (const void *buffer, int size);
void usb_receivebuffer (void *buffer, int size);
void usb_sendbuffersafe (const void *buffer, int size);
void usb_receivebuffersafe (void *buffer, int size);
#endif

#endif // __USB_H__
