/*---------------------------------------------------------------------------------------------
 * USB Gecko Development Kit - http://www.usbgecko.com
 * --------------------------------------------------------------------------------------------
 * 
 *
 * usb.c - V1.2 functions for the USB Gecko adapter (www.usbgecko.com).
 * Now works for Wii Mode - use WIIMODE define in usb.h to set
 * Copyright (c) 2008 - Nuke - <wiinuke@gmail.com>
 * 
 *---------------------------------------------------------------------------------------------*/

typedef int s32;
typedef unsigned int u32;
#include "usb.h"

#ifdef DEBUG

/*---------------------------------------------------------------------------------------------*
    Name:           usb_sendbyte
    Description:	Send byte to Gamecube/Wii over EXI memory card port
*----------------------------------------------------------------------------------------------*/

static int __usb_sendbyte (char sendbyte)
{
	s32 i;
	
	exi_chan1sr = 0x000000D0;						
	exi_chan1data = 0xB0000000 | (sendbyte<<20);	
	exi_chan1cr = 0x19;								
	while((exi_chan1cr)&1);							
	i = exi_chan1data;								
	exi_chan1sr = 0;  
	if (i&0x04000000){
		return 1;									
	}   
    return 0;										
}

/*---------------------------------------------------------------------------------------------*
    Name:           usb_receivebyte
    Description:	Receive byte from Gamecube/Wii over EXI memory card port
*----------------------------------------------------------------------------------------------*/

static int __usb_receivebyte (char *receivebyte)
{
	s32 i = 0;

	exi_chan1sr = 0x000000D0;			
	exi_chan1data = 0xA0000000;		
	exi_chan1cr = 0x19;					
	while((exi_chan1cr)&1);				
	i = exi_chan1data;					
	exi_chan1sr = 0;  
	if (i&0x08000000){
	    *receivebyte=(i>>16)&0xff;		
	    return 1;						
	} 
	return 0;							
}

/*---------------------------------------------------------------------------------------------*
    Name:           usb_checksendstatus
    Description:	Chesk the FIFO is ready to send
*----------------------------------------------------------------------------------------------*/

static int __usb_checksendstatus()
{
	s32 i = 0;

	exi_chan1sr = 0x000000D0;					
	exi_chan1data = 0xC0000000;						
	exi_chan1cr = 0x19;								
	while((exi_chan1cr)&1);							
	i = exi_chan1data;									
	exi_chan1sr = 0x0;  
	if (i&0x04000000){
		return 1;									
	}   
    return 0;										
}

/*---------------------------------------------------------------------------------------------*
    Name:           usb_checkreceivestatus
    Description:	Check the FIFO is ready to receive
*----------------------------------------------------------------------------------------------*/

static int __usb_checkreceivestatus()
{
	s32 i = 0;
	exi_chan1sr = 0x000000D0;						
	exi_chan1data = 0xD0000000;						
	exi_chan1cr = 0x19;								
	while((exi_chan1cr)&1);							   	
	i = exi_chan1data;								
	exi_chan1sr = 0x0;  
	if (i&0x04000000){
		return 1;								
	}   
    return 0;										
}

/*---------------------------------------------------------------------------------------------*
    Name:           usb_sendbuffer
    Description:	Simple buffer send routine
*----------------------------------------------------------------------------------------------*/

void usb_sendbuffer (const void *buffer, int size)
{
	char *sendbyte = (char*) buffer;
	s32 bytesleft = size;					
	s32 returnvalue;

	while (bytesleft  > 0)
	{
		returnvalue = __usb_sendbyte(*sendbyte);
		if(returnvalue) {							
			sendbyte++;							
			bytesleft--;
		}
	}
}

/*---------------------------------------------------------------------------------------------*
    Name:           usb_receivebuffer
    Description:	Simple buffer receive routine
*----------------------------------------------------------------------------------------------*/

void usb_receivebuffer (void *buffer, int size)
{
	char *receivebyte = (char*)buffer;
	s32 bytesleft = size;						
	s32 returnvalue;

	while (bytesleft > 0)
	{
		returnvalue = __usb_receivebyte(receivebyte);	
		if(returnvalue) {							
			receivebyte++;							
			bytesleft--;							
		}
	}
}

/*---------------------------------------------------------------------------------------------*
    Name:           usb_sendbuffersafe
    Description:	Simple buffer send routine with fifo check (use for large transfers)
*----------------------------------------------------------------------------------------------*/

void usb_sendbuffersafe (const void *buffer, int size)
{
	char *sendbyte = (char*) buffer;
	s32 bytesleft = size;					
	s32 returnvalue;

	while (bytesleft  > 0)
	{
		if(__usb_checksendstatus()){
			returnvalue = __usb_sendbyte(*sendbyte);		
			if(returnvalue) {							
				sendbyte++;							
				bytesleft--;						
			}
		}
	}
}

/*---------------------------------------------------------------------------------------------*
    Name:           usb_receivebuffersafe
    Description:	Simple buffer receive routine with fifo check (use for large transfers)
*----------------------------------------------------------------------------------------------*/

void usb_receivebuffersafe (void *buffer, int size)
{
	char *receivebyte = (char*)buffer;
	s32 bytesleft = size;						
	s32 returnvalue;

	while (bytesleft > 0)
	{
		if(__usb_checkreceivestatus()){
			returnvalue = __usb_receivebyte(receivebyte);	
			if(returnvalue) {							
				receivebyte++;							
				bytesleft--;						
			}
		}
	}
}

/*---------------------------------------------------------------------------------------------*
    Name:           usb_checkgecko
    Description:	Chesk the Gecko is connected
*----------------------------------------------------------------------------------------------*/
int usb_checkgecko()
{
	s32 i = 0;

	exi_chan1sr = 0x000000D0;						
	exi_chan1data = 0x90000000;						
	exi_chan1cr = 0x19;								
	while((exi_chan1cr)&1);							
	i = exi_chan1data;									
	exi_chan1sr = 0x0; 
	if (i==0x04700000){
		return 1;									
	}   
    return 0;									
}

/*---------------------------------------------------------------------------------------------*
    Name:           usb_flush
    Description:	Flushes the FIFO, Use at the start of your program to avoid trash
*----------------------------------------------------------------------------------------------*/

void usb_flush()
{
 char tempbyte;

 while (__usb_receivebyte(&tempbyte));
}
#endif
