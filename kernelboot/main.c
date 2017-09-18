/*
Nintendont Kernelboot

Copyright (C) 2017 FIX94

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
#include "syscalls.h"

//called on ES Ioctl 0x1F
void _main()
{
	//Just a jump at this location to our set entry
	void *threadVirt = 0;
	if(*(volatile unsigned int*)0x20109740 == 0xE59F1004)
		threadVirt = (void*)0x20109740; //Address on Wii 
	else if(*(volatile unsigned int*)0x2010999C == 0xE59F1004)
		threadVirt = (void*)0x2010999C; //Address on WiiU
	//Will be used by threadVirt
	*(volatile unsigned int*)0x12FFFFE0 = 0x12F00000; //Nintendont Entry
	sync_after_write((void*)0x12FFFFE0, 0x20);
	//Start Thread
	int ret = thread_create(threadVirt,(void*)0,(void*)(0x12FA6000+0x2000),(0x2000>>2),0x78,1);
	thread_continue(ret);
}
