#include "../../../common/include/CommonConfig.h"
#include "global.h"
#include "HID.h"
#include "hidmem.h"
#define PAD_CHAN0_BIT				0x80000000

static u32 chan, MaxPads;
static vu32 PADButtonsStick, PADTriggerCStick;
static u32 stubsize = 0x1800;
static vu32 *stubdest = (u32*)0xC1330000;
static vu32 *stubsrc = (u32*)0xD3011810;
static vu16* const _dspReg = (u16*)0xCC005000;
static vu32* const _siReg = (u32*)0xCD006400;
static vu32* const MotorCommand = (u32*)0xD3003010;
static vu32* reset = (u32*)0xC0002F54;
static vu32* HIDPad = (u32*)0xD3002700;
const s8 DEADZONE = 0x1A;
#define HID_PAD_NONE	4
#define HID_PAD_NOT_SET	0xFF
void _start()
{
	// Registers r1,r13-r31 automatically restored if used.
	// Registers r0, r3-r12 should be handled by calling function
	// Register r2 not changed
	PADStatus *Pad = (PADStatus*)(0x93002800); //PadBuff
	MaxPads = ((NIN_CFG*)0xD3002900)->MaxPads;
	if ((MaxPads > NIN_CFG_MAXPAD) || (MaxPads == 0))
		MaxPads = NIN_CFG_MAXPAD;
	u8 HIDPad = ((((NIN_CFG*)0xD3002900)->Config) & NIN_CFG_HID) == 0 ? HID_PAD_NONE : HID_PAD_NOT_SET;
	// Allow 0 MaxPads above instead?
	if ( (*(u32*)(0xCD8005A0) >> 16 ) == 0xCAFE ) // Wii U
	{
		MaxPads = 0;
		HIDPad = 0;
	}
	for (chan = 0; chan < MaxPads; ++chan)
	{
		/* transfer the actual data */
		u32 x;
		u32 addr = 0xCD006400 + (0x0c * chan);
		asm volatile("lwz %0,0(%1) ; sync" : "=r"(x) : "b"(addr));
		//we just needed the first read to clear the status
		asm volatile("lwz %0,4(%1) ; sync" : "=r"(x) : "b"(addr));
		PADButtonsStick = x;
		asm volatile("lwz %0,8(%1) ; sync" : "=r"(x) : "b"(addr));
		PADTriggerCStick = x;
		/* convert data to PADStatus */
		Pad[chan].button = ((PADButtonsStick>>16)&0xFFFF);
		if(Pad[chan].button & 0x8000) /* controller not enabled */
		{
			u32 psize = sizeof(PADStatus);
			u8 *CurPad = (u8*)(&Pad[chan]);
			while(psize--) *CurPad++ = 0;
			if(HIDPad == HID_PAD_NOT_SET)
			{
				HIDPad = chan;
				continue;
			}
			Pad[chan].err = -1;
			continue;
		}
		Pad[chan].err = 0;
		Pad[chan].stickX = ((PADButtonsStick>>8)&0xFF)-128;
		Pad[chan].stickY = ((PADButtonsStick>>0)&0xFF)-128;
		Pad[chan].substickX = ((PADTriggerCStick>>24)&0xFF)-128;
		Pad[chan].substickY = ((PADTriggerCStick>>16)&0xFF)-128;

		/* Calculate left trigger with deadzone */
		u8 tmp_triggerL = ((PADTriggerCStick>>8)&0xFF);
		if(tmp_triggerL > DEADZONE)
			Pad[chan].triggerLeft = (tmp_triggerL - DEADZONE) * 1.11f;
		else
			Pad[chan].triggerLeft = 0;
		/* Calculate right trigger with deadzone */
		u8 tmp_triggerR = ((PADTriggerCStick>>0)&0xFF);
		if(tmp_triggerR > DEADZONE)
			Pad[chan].triggerRight = (tmp_triggerR - DEADZONE) * 1.11f;
		else
			Pad[chan].triggerRight = 0;

		/* shutdown by pressing B,Z,R,PAD_BUTTON_DOWN */
		if((Pad[chan].button&0x234) == 0x234)
		{
			/* stop audio dma */
			_dspReg[27] = (_dspReg[27]&~0x8000);
			/* reset status 1 */
			*reset = 0x1DEA;
			while(*reset == 0x1DEA) ;
			/* load in stub */
			u32 a = (u32)stubdest;
			u32 end = (u32)(stubdest + stubsize);
			for ( ; a < end; a += 32)
			{
				u8 b;
				for(b = 0; b < 4; ++b)
					*stubdest++ = *stubsrc++;
				__asm("dcbi 0,%0 ; sync ; icbi 0,%0" : : "b"(a));
			}
			__asm(
				"sync ; isync\n"
				"lis %r3, 0x8133\n"
				"mtlr %r3\n"
				"blr\n"
			);
		}
		/* clear unneeded button attributes */
		Pad[chan].button &= 0x9F7F;
		/* set current command */
		_siReg[chan*3] = (MotorCommand[chan]&0x3) | 0x00400300;
		/* transfer command */
		_siReg[14] |= (1<<31);
		while(_siReg[14] & (1<<31));
	}

	if (HIDPad < HID_PAD_NONE)
	{
		chan = HIDPad;
		if(HID_CTRL->Power.Mask &&	//shutdown if power configured and all power buttons pressed
		((HID_Packet[HID_CTRL->Power.Offset] & HID_CTRL->Power.Mask) == HID_CTRL->Power.Mask))
		{
			/* stop audio dma */
			_dspReg[27] = (_dspReg[27]&~0x8000);
			/* reset status 1 */
			*reset = 0x1DEA;
			while(*reset == 0x1DEA) ;
			/* load in stub */
			u32 a = (u32)stubdest;
			u32 end = (u32)(stubdest + stubsize);
			for ( ; a < end; a += 32)
			{
				u8 b;
				for(b = 0; b < 4; ++b)
					*stubdest++ = *stubsrc++;
				__asm("dcbi 0,%0 ; sync ; icbi 0,%0" : : "b"(a));
			}
			__asm(
				"sync ; isync\n"
				"lis %r3, 0x8133\n"
				"mtlr %r3\n"
				"blr\n"
			);
		}
		Pad[chan].err = 0;
	
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
		Pad[chan].button = button;
	
		/* then analog sticks */
		s8 stickX, stickY, substickX, substickY;
		if ((HID_CTRL->VID == 0x044F) && (HID_CTRL->PID == 0xB303))	//Logitech Thrustmaster Firestorm Dual Analog 2
		{
			stickX		= HID_Packet[HID_CTRL->StickX.Offset];			//raw 80 81...FF 00 ... 7E 7F (left...center...right)
			stickY		= -1 - HID_Packet[HID_CTRL->StickY.Offset];		//raw 80 81...FF 00 ... 7E 7F (up...center...down)
			substickX	= HID_Packet[HID_CTRL->CStickX.Offset];			//raw 80 81...FF 00 ... 7E 7F (left...center...right)
			substickY	= 127 - HID_Packet[HID_CTRL->CStickY.Offset];	//raw 00 01...7F 80 ... FE FF (up...center...down)
		}
		else
		if ((HID_CTRL->VID == 0x0926) && (HID_CTRL->PID == 0x2526))	//Mayflash 3 in 1 Magic Joy Box 
		{
			stickX		= HID_Packet[HID_CTRL->StickX.Offset] - 128;	//raw 1A 1B...80 81 ... E4 E5 (left...center...right)
			stickY		= 127 - HID_Packet[HID_CTRL->StickY.Offset];	//raw 0E 0F...7E 7F ... E4 E5 (up...center...down)
			if (HID_Packet[HID_CTRL->CStickX.Offset] >= 0)
				substickX	= (HID_Packet[HID_CTRL->CStickX.Offset] * 2) - 128;	//raw 90 91 10 11...41 42...68 69 EA EB (left...center...right) the 90 91 EA EB are hard right and left almost to the point of breaking
			else if (HID_Packet[HID_CTRL->CStickX.Offset] < 0xD0)
				substickX	= 0xFE;
			else
				substickX	= 0;
			substickY	= 127 - ((HID_Packet[HID_CTRL->CStickY.Offset] - 128) * 4);	//raw 88 89...9E 9F A0 A1 ... BA BB (up...center...down)
		}
		else	//standard sticks
		{
			stickX		= HID_Packet[HID_CTRL->StickX.Offset] - 128;
			stickY		= 127 - HID_Packet[HID_CTRL->StickY.Offset];
			substickX	= HID_Packet[HID_CTRL->CStickX.Offset] - 128;
			substickY	= 127 - HID_Packet[HID_CTRL->CStickY.Offset];
		}
	
		s8 tmp_stick = 0;
		if(stickX > HID_CTRL->StickX.DeadZone && stickX > 0)
			tmp_stick = (double)(stickX - HID_CTRL->StickX.DeadZone) * HID_CTRL->StickX.Radius / 1000;
		else if(stickX < -HID_CTRL->StickX.DeadZone && stickX < 0)
			tmp_stick = (double)(stickX + HID_CTRL->StickX.DeadZone) * HID_CTRL->StickX.Radius / 1000;
		Pad[chan].stickX = tmp_stick;
	
		tmp_stick = 0;
		if(stickY > HID_CTRL->StickY.DeadZone && stickY > 0)
			tmp_stick = (double)(stickY - HID_CTRL->StickY.DeadZone) * HID_CTRL->StickY.Radius / 1000;
		else if(stickY < -HID_CTRL->StickY.DeadZone && stickY < 0)
			tmp_stick = (double)(stickY + HID_CTRL->StickY.DeadZone) * HID_CTRL->StickY.Radius / 1000;
		Pad[chan].stickY = tmp_stick;
	
		tmp_stick = 0;
		if(substickX > HID_CTRL->CStickX.DeadZone && substickX > 0)
			tmp_stick = (double)(substickX - HID_CTRL->CStickX.DeadZone) * HID_CTRL->CStickX.Radius / 1000;
		else if(substickX < -HID_CTRL->CStickX.DeadZone && substickX < 0)
			tmp_stick = (double)(substickX + HID_CTRL->CStickX.DeadZone) * HID_CTRL->CStickX.Radius / 1000;
		Pad[chan].substickX = tmp_stick;
	
		tmp_stick = 0;
		if(substickY > HID_CTRL->CStickY.DeadZone && substickY > 0)
			tmp_stick = (double)(substickY - HID_CTRL->CStickY.DeadZone) * HID_CTRL->CStickY.Radius / 1000;
		else if(substickY < -HID_CTRL->CStickY.DeadZone && substickY < 0)
			tmp_stick = (double)(substickY + HID_CTRL->CStickY.DeadZone) * HID_CTRL->CStickY.Radius / 1000;
		Pad[chan].substickY = tmp_stick;
/*
		Pad[chan].stickX = stickX;
		Pad[chan].stickY = stickY;
		Pad[chan].substickX = substickX;
		Pad[chan].substickY = substickY;
*/
		/* then triggers */
		if( HID_CTRL->DigitalLR )
		{	/* digital triggers, not much to do */
			if(Pad[chan].button & PAD_TRIGGER_L)
				Pad[chan].triggerLeft = 255;
			else
				Pad[chan].triggerLeft = 0;
			if(Pad[chan].button & PAD_TRIGGER_R)
				Pad[chan].triggerRight = 255;
			else
				Pad[chan].triggerRight = 0;
		}
		else
		{	/* much to do with analog */
			u8 tmp_triggerL = 0;
			u8 tmp_triggerR = 0;
			if ((HID_CTRL->VID == 0x0926) && (HID_CTRL->PID == 0x2526))	//Mayflash 3 in 1 Magic Joy Box 
			{
				tmp_triggerL =  HID_Packet[HID_CTRL->LAnalog] & 0xF0;	//high nibble raw 1x 2x ... Dx Ex 
				tmp_triggerR = (HID_Packet[HID_CTRL->RAnalog] & 0x0F) * 16 ;	//low nibble raw x1 x2 ...xD xE
				if(Pad[chan].button & PAD_TRIGGER_L)
					tmp_triggerL = 255;
				if(Pad[chan].button & PAD_TRIGGER_R)
					tmp_triggerR = 255;
			}
			else	//standard analog triggers
			{
				tmp_triggerL = HID_Packet[HID_CTRL->LAnalog];
				tmp_triggerR = HID_Packet[HID_CTRL->RAnalog];
			}
			/* Calculate left trigger with deadzone */
			if(tmp_triggerL > DEADZONE)
				Pad[chan].triggerLeft = (tmp_triggerL - DEADZONE) * 1.11f;
			else
				Pad[chan].triggerLeft = 0;
			/* Calculate right trigger with deadzone */
			if(tmp_triggerR > DEADZONE)
				Pad[chan].triggerRight = (tmp_triggerR - DEADZONE) * 1.11f;
			else
				Pad[chan].triggerRight = 0;
		}
	}

	return;
}