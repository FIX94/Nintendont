#include "global.h"
#include "../../../../common/include/CommonConfig.h"
#include "HID.h"
#include "hidmem.h"
static unsigned int stubsize = 0x10000;
static char *stubdest = (char*)0x80001800;
static char *stubsrc = (char*)0x92010010;
static volatile unsigned short* const _dspReg = (unsigned short*)0xCC005000;
static char *a;
unsigned int regs[29];
const s8 DEADZONE = 0x1A;
void _start()
{
	asm volatile(
		"lis %r6, regs@h\n"
		"ori %r6, %r6, regs@l\n"
		"stw %r0, 0(%r6)\n"
		"stw %r1, 4(%r6)\n"
		"stw %r2, 8(%r6)\n"
		"stw %r3, 12(%r6)\n"
		"stw %r4, 16(%r6)\n"
		"stw %r5, 20(%r6)\n"
		"stw %r9, 24(%r6)\n"
		"stw %r10, 28(%r6)\n"
		"stw %r11, 32(%r6)\n"
		"stw %r12, 36(%r6)\n"
		"stw %r13, 40(%r6)\n"
		"stw %r14, 44(%r6)\n"
		"stw %r15, 48(%r6)\n"
		"stw %r16, 52(%r6)\n"
		"stw %r17, 56(%r6)\n"
		"stw %r18, 60(%r6)\n"
		"stw %r19, 64(%r6)\n"
		"stw %r20, 68(%r6)\n"
		"stw %r21, 72(%r6)\n"
		"stw %r22, 76(%r6)\n"
		"stw %r23, 80(%r6)\n"
		"stw %r24, 84(%r6)\n"
		"stw %r25, 88(%r6)\n"
		"stw %r26, 92(%r6)\n"
		"stw %r27, 96(%r6)\n"
		"stw %r28, 100(%r6)\n"
		"stw %r29, 104(%r6)\n"
		"stw %r30, 108(%r6)\n"
		"stw %r31, 112(%r6)\n"
	);

	if(HID_CTRL->Power.Mask &&	//shutdown if power configured and all power buttons pressed
	  ((HID_Packet[HID_CTRL->Power.Offset] & HID_CTRL->Power.Mask) == HID_CTRL->Power.Mask))
	{
		/* stop audio dma */
		_dspReg[27] = (_dspReg[27]&~0x8000);
		/* reset status 1 */
		volatile unsigned int* reset = (volatile unsigned int*)0x9200300C;
		*reset = 1;
		__asm("dcbf 0,%0" : : "b"(reset));
		__asm("dcbst 0,%0 ; sync ; icbi 0,%0" : : "b"(reset));
		__asm("sync ; isync");
		while(*reset == 1)
		{
			__asm("dcbi 0,%0" : : "b"(reset) : "memory");
			__asm("sync ; isync");
		}
		/* kernel accepted, load in stub */
		while(stubsize--) *stubdest++ = *stubsrc++;
		for (a = (char*)0x80001800; a < (char*)0x8000F000; a += 32)
		{
			__asm("dcbf 0,%0" : : "b"(a));
			__asm("dcbst 0,%0 ; sync ; icbi 0,%0" : : "b"(a));
		}
		__asm(
			"sync ; isync\n"
			"lis %r3, 0x8000\n"
			"ori %r3, %r3, 0xC000\n"
			"mtlr %r3\n"
			"blr\n"
		);
		return;
	}

	PADStatus *Pad = (PADStatus*)(((char*)regs[5])-0x30); //r5=return, 0x30 buffer before it
	Pad[0].err = 0;
	Pad[1].err = -1;	// NO controller
	Pad[2].err = -1;
	Pad[3].err = -1;

	/* first buttons */
	u16 button = 0;
	if(HID_CTRL->DPAD == 0)
	{
		if( HID_Packet[HID_CTRL->Left.Offset] & HID_CTRL->Left.Mask )
			button |= PAD_BUTTON_LEFT;

		if( HID_Packet[HID_CTRL->Right.Offset] & HID_CTRL->Right.Mask )
			button |= PAD_BUTTON_RIGHT;

		if( HID_Packet[HID_CTRL->Down.Offset] & HID_CTRL->Down.Mask )
			button |= PAD_BUTTON_DOWN;

		if( HID_Packet[HID_CTRL->Up.Offset] & HID_CTRL->Up.Mask )
			button |= PAD_BUTTON_UP;
	}
	else
	{
		if((HID_Packet[HID_CTRL->Up.Offset] == HID_CTRL->Up.Mask)||(HID_Packet[HID_CTRL->UpLeft.Offset] == HID_CTRL->UpLeft.Mask)||(HID_Packet[HID_CTRL->RightUp.Offset]	== HID_CTRL->RightUp.Mask))
			button |= PAD_BUTTON_UP;

		if(	(HID_Packet[HID_CTRL->Right.Offset] == HID_CTRL->Right.Mask)||(HID_Packet[HID_CTRL->DownRight.Offset] == HID_CTRL->DownRight.Mask)||(HID_Packet[HID_CTRL->RightUp.Offset] == HID_CTRL->RightUp.Mask))
			button |= PAD_BUTTON_RIGHT;

		if((HID_Packet[HID_CTRL->Down.Offset] == HID_CTRL->Down.Mask) ||(HID_Packet[HID_CTRL->DownRight.Offset] == HID_CTRL->DownRight.Mask) ||(HID_Packet[HID_CTRL->DownLeft.Offset] == HID_CTRL->DownLeft.Mask))
			button |= PAD_BUTTON_DOWN;

		if((HID_Packet[HID_CTRL->Left.Offset] == HID_CTRL->Left.Mask) || (HID_Packet[HID_CTRL->DownLeft.Offset] == HID_CTRL->DownLeft.Mask) || (HID_Packet[HID_CTRL->UpLeft.Offset] == HID_CTRL->UpLeft.Mask) )
			button |= PAD_BUTTON_LEFT;
	}
	if(HID_Packet[HID_CTRL->A.Offset] & HID_CTRL->A.Mask)
		button |= PAD_BUTTON_A;
	if(HID_Packet[HID_CTRL->B.Offset] & HID_CTRL->B.Mask)
		button |= PAD_BUTTON_B;
	if(HID_Packet[HID_CTRL->X.Offset] & HID_CTRL->X.Mask)
		button |= PAD_BUTTON_X;
	if(HID_Packet[HID_CTRL->Y.Offset] & HID_CTRL->Y.Mask)
		button |= PAD_BUTTON_Y;
	if(HID_Packet[HID_CTRL->Z.Offset] & HID_CTRL->Z.Mask)
		button |= PAD_TRIGGER_Z;
	if(HID_Packet[HID_CTRL->L.Offset] & HID_CTRL->L.Mask)
		button |= PAD_TRIGGER_L;
	if(HID_Packet[HID_CTRL->R.Offset] & HID_CTRL->R.Mask)
		button |= PAD_TRIGGER_R;
	if(HID_Packet[HID_CTRL->S.Offset] & HID_CTRL->S.Mask)
		button |= PAD_BUTTON_START;
	Pad[0].button = button;

	/* then analog sticks */
	s8 stickX, stickY, substickX, substickY;
	if ((HID_CTRL->VID == 0x044F) && (HID_CTRL->PID == 0xB303))	//Logitech Thrustmaster Firestorm Dual Analog 2
	{
		stickX		= HID_Packet[HID_CTRL->StickX];			//raw 80 81...FF 00 ... 7E 7F (left...center...right)
		stickY		= -1 - HID_Packet[HID_CTRL->StickY];	//raw 80 81...FF 00 ... 7E 7F (up...center...down)
		substickX	= HID_Packet[HID_CTRL->CStickX];		//raw 80 81...FF 00 ... 7E 7F (left...center...right)
		substickY	= 127 - HID_Packet[HID_CTRL->CStickY];	//raw 00 01...7F 80 ... FE FF (up...center...down)
	}
	else
	if ((HID_CTRL->VID == 0x0926) && (HID_CTRL->PID == 0x2526))	//Mayflash 3 in 1 Magic Joy Box 
	{
		stickX		= HID_Packet[HID_CTRL->StickX] - 128;	//raw 1A 1B...80 81 ... E4 E5 (left...center...right)
		stickY		= 127 - HID_Packet[HID_CTRL->StickY];	//raw 0E 0F...7E 7F ... E4 E5 (up...center...down)
		if (HID_Packet[HID_CTRL->CStickX] >= 0)
			substickX	= (HID_Packet[HID_CTRL->CStickX] * 2) - 128;	//raw 90 91 10 11...41 42...68 69 EA EB (left...center...right) the 90 91 EA EB are hard right and left almost to the point of breaking
		else if (HID_Packet[HID_CTRL->CStickX] < 0xD0)
			substickX	= 0xFE;
		else
			substickX	= 0;
		substickY	= 127 - ((HID_Packet[HID_CTRL->CStickY] - 128) * 4);	//raw 88 89...9E 9F A0 A1 ... BA BB (up...center...down)
	}
	else	//standard sticks
	{
		stickX		= HID_Packet[HID_CTRL->StickX] - 128;
		stickY		= 127 - HID_Packet[HID_CTRL->StickY];
		substickX	= HID_Packet[HID_CTRL->CStickX] - 128;
		substickY	= 127 - HID_Packet[HID_CTRL->CStickY];
	}

	s8 tmp_stick = 0;
	if(stickX > DEADZONE && stickX > 0)
		tmp_stick = (stickX - DEADZONE) * 1.25f;
	else if(stickX < -DEADZONE && stickX < 0)
		tmp_stick = (stickX + DEADZONE) * 1.25f;
	Pad[0].stickX = tmp_stick;

	tmp_stick = 0;
	if(stickY > DEADZONE && stickY > 0)
		tmp_stick = (stickY - DEADZONE) * 1.25f;
	else if(stickY < -DEADZONE && stickY < 0)
		tmp_stick = (stickY + DEADZONE) * 1.25f;
	Pad[0].stickY = tmp_stick;

	tmp_stick = 0;
	if(substickX > DEADZONE && substickX > 0)
		tmp_stick = (substickX - DEADZONE) * 1.25f;
	else if(substickX < -DEADZONE && substickX < 0)
		tmp_stick = (substickX + DEADZONE) * 1.25f;
	Pad[0].substickX = tmp_stick;

	tmp_stick = 0;
	if(substickY > DEADZONE && substickY > 0)
		tmp_stick = (substickY - DEADZONE) * 1.25f;
	else if(substickY < -DEADZONE && substickY < 0)
		tmp_stick = (substickY + DEADZONE) * 1.25f;
	Pad[0].substickY = tmp_stick;

	/* then triggers */
	u8 tmp_triggerL = 0;
	u8 tmp_triggerR = 0;
	if( HID_CTRL->DigitalLR )
	{
		if(Pad[0].button & PAD_TRIGGER_L)
			tmp_triggerL = 255;
		if(Pad[0].button & PAD_TRIGGER_R)
			tmp_triggerR = 255;
	}
	else
	if ((HID_CTRL->VID == 0x0926) && (HID_CTRL->PID == 0x2526))	//Mayflash 3 in 1 Magic Joy Box 
	{
		tmp_triggerL =  HID_Packet[HID_CTRL->LAnalog] & 0xF0;	//high nibble raw 1x 2x ... Dx Ex 
		tmp_triggerR = (HID_Packet[HID_CTRL->RAnalog] & 0x0F) * 16 ;	//low nibble raw x1 x2 ...xD xE
		if(Pad[0].button & PAD_TRIGGER_L)
			tmp_triggerL = 255;
		if(Pad[0].button & PAD_TRIGGER_R)
			tmp_triggerR = 255;
	}
	else	//standard analog triggers
	{
		tmp_triggerL = HID_Packet[HID_CTRL->LAnalog];
		tmp_triggerR = HID_Packet[HID_CTRL->RAnalog];
	}
	Pad[0].triggerLeft = (tmp_triggerL > 0x24 ? tmp_triggerL : 0);		//apply DEADZONE
	Pad[0].triggerRight = (tmp_triggerR > 0x24 ? tmp_triggerR : 0);	//apply DEADZONE

	asm volatile(
		"lis %r6, regs@h\n"
		"ori %r6, %r6, regs@l\n"
		"lwz %r0, 0(%r6)\n"
		"lwz %r1, 4(%r6)\n"
		"lwz %r2, 8(%r6)\n"
		"lwz %r3, 12(%r6)\n"
		"lwz %r4, 16(%r6)\n"
		"lwz %r5, 20(%r6)\n"
		"lwz %r9, 24(%r6)\n"
		"lwz %r10, 28(%r6)\n"
		"lwz %r11, 32(%r6)\n"
		"lwz %r12, 36(%r6)\n"
		"lwz %r13, 40(%r6)\n"
		"lwz %r14, 44(%r6)\n"
		"lwz %r15, 48(%r6)\n"
		"lwz %r16, 52(%r6)\n"
		"lwz %r17, 56(%r6)\n"
		"lwz %r18, 60(%r6)\n"
		"lwz %r19, 64(%r6)\n"
		"lwz %r20, 68(%r6)\n"
		"lwz %r21, 72(%r6)\n"
		"lwz %r22, 76(%r6)\n"
		"lwz %r23, 80(%r6)\n"
		"lwz %r24, 84(%r6)\n"
		"lwz %r25, 88(%r6)\n"
		"lwz %r26, 92(%r6)\n"
		"lwz %r27, 96(%r6)\n"
		"lwz %r28, 100(%r6)\n"
		"lwz %r29, 104(%r6)\n"
		"lwz %r30, 108(%r6)\n"
		"lwz %r31, 112(%r6)\n"
		"mtlr %r5\n"
	);
}
