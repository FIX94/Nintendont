/*
SI.c for Nintendont (Kernel)

Copyright (C) 2014 FIX94

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
#include "SI.h"
#include "debug.h"
#include "string.h"

bool SI_IRQ = false;
bool complete = true;
u32 cur_control = 0, cur_status = 0, prev_control = 0, prev_status = 0;
u8 cur_chan = 0, prev_chan = 0;
void SIInit()
{
	memset((void*)SI_BASE, 0, 0x100);
	sync_after_write((void*)SI_BASE, 0x100);
}

void SIInterrupt()
{
	if(complete)
	{
		cur_control &= ~(1<<27); //mask read status interupt
		cur_control &= ~(1<<28); //no read interrupt requested anymore, completed transfer
		/* Exectued commands */
		cur_control &= ~1;
	}
	else
	{
		cur_control |= (1<<27); //enable read status interupt
		cur_control |= (1<<28); //read status interupt request, might need more if we wanna use that
	}
	write32(SI_CONTROL, cur_control);
	sync_after_write((void*)SI_CONTROL, 4);

	write32( 0x10, 0 );
	write32( 0x14, 0x8 );		// SI IRQ
	sync_after_write( (void*)0x10, 8 );
	write32( HW_IPC_ARMCTRL, (1<<0) | (1<<4) ); //throw irq

	complete ^= 1;
}

void SIUpdateRegisters()
{
	sync_before_read((void*)SI_BASE, 0x40);
	cur_control = read32(SI_CONTROL);
	//cur_status = read32(SI_STATUS);
	if(cur_control != prev_control)
	{
		//dbgprintf("Read SI Control: %08x\r\n", cur_control);
		if(cur_control & (1<<30) && cur_control & (1<<0)) //enable interrupts and transfer
		{
			cur_control |= (1<<29); //we normally always have some communication error?
			/* set controller responses, not needed with patched PADRead
			if((cur_control & 7) == 3) //chan 1
				cur_status |= (1<<19);
			else if((cur_control & 7) == 5) //chan 2
				cur_status |= (1<<11);
			else if((cur_control & 7) == 7) //chan 3
				cur_status |= (1<<3);
			//dbgprintf("New SI Control: %08x\r\n", cur_control);
			write32(SI_STATUS, cur_status);
			sync_after_write((void*)SI_STATUS, 4);*/
			write32(SI_CONTROL, cur_control);
			sync_after_write((void*)SI_CONTROL, 4);
			SI_IRQ = true; //we will give the game regular updates
		}
		prev_control = cur_control;
		if((cur_control & 0x7F0000) != 0)
			complete = true; //requested some bytes
		cur_chan = (cur_control & 6) >> 1;
		if(cur_chan != prev_chan) //force repeat
		{
			complete = true;
			prev_chan = cur_chan;
		}
	}
	/*if(cur_status != prev_status)
	{
		//dbgprintf("Read SI Status: %08x\r\n", cur_status);
		prev_status = cur_status;
	}*/
}
