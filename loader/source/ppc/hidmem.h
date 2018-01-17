
#ifndef _HIDMEM_H_
#define _HIDMEM_H_
static volatile controller *HID_CTRL = (volatile controller*)0x93005000;
static vu8 *HID_Packet = (vu8*)0x930050F0;
#endif
