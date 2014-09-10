#include "../../../common/include/CommonConfig.h"
#include "global.h"
#include "HID.h"
#include "hidmem.h"
#define PAD_CHAN0_BIT				0x80000000

static u32 stubsize = 0x1800;
static vu32 *stubdest = (u32*)0x81330000;
static vu32 *stubsrc = (u32*)0x93011810;
static vu16* const _dspReg = (u16*)0xCC005000;
static vu32* const _siReg = (u32*)0xCD006400;
static vu32* const MotorCommand = (u32*)0xD3003010;
static vu32* reset = (u32*)0xC0002F54;
static vu32* HIDMotor = (u32*)0x93002700;
static vu32* PadUsed = (u32*)0x93002704;

static vu32* PADIsBarrel = (u32*)0xD3002830;
static vu32* PADBarrelEnabled = (u32*)0xD3002840;
static vu32* PADBarrelPress = (u32*)0xD3002850;

struct BTPadCont *BTPad = (struct BTPadCont*)0x932F0000;
static vu32* BTMotor = (u32*)0x93002720;
static vu32* BTPadFree = (u32*)0x93002730;

static u32 PrevAdapterChannel1 = 0;
static u32 PrevAdapterChannel2 = 0;
static u32 PrevAdapterChannel3 = 0;
static u32 PrevAdapterChannel4 = 0;

const s8 DEADZONE = 0x1A;
#define HID_PAD_NONE	4
#define HID_PAD_NOT_SET	0xFF

#define C_NOT_SET	(0<<0)
#define C_CCP		(1<<0)
#define C_CC		(1<<1)
#define C_SWAP		(1<<2)
#define C_RUMBLE_WM	(1<<3)

u32 _start()
{
	// Registers r1,r13-r31 automatically restored if used.
	// Registers r0, r3-r12 should be handled by calling function
	// Register r2 not changed
	u32 Rumble = 0, memInvalidate, memFlush;
	u32 used = 0;

	PADStatus *Pad = (PADStatus*)(0x93002800); //PadBuff
	u32 MaxPads = ((NIN_CFG*)0xD3002900)->MaxPads;
	if (MaxPads > NIN_CFG_MAXPAD)
		MaxPads = NIN_CFG_MAXPAD;

	u32 HIDPad = ((((NIN_CFG*)0xD3002900)->Config) & NIN_CFG_HID) == 0 ? HID_PAD_NONE : HID_PAD_NOT_SET;
	u32 chan;
	for (chan = 0; (chan < MaxPads); ++chan)
	{
		/* transfer the actual data */
		u32 x, PADButtonsStick, PADTriggerCStick;
		u32 addr = 0xCD006400 + (0x0c * chan);
		asm volatile("lwz %0,0(%1) ; sync" : "=r"(x) : "b"(addr));
		//we just needed the first read to clear the status
		asm volatile("lwz %0,4(%1) ; sync" : "=r"(PADButtonsStick) : "b"(addr));
		asm volatile("lwz %0,8(%1) ; sync" : "=r"(PADTriggerCStick) : "b"(addr));
		/* convert data to PADStatus */
		Pad[chan].button = ((PADButtonsStick>>16)&0xFFFF);
		if(Pad[chan].button & 0x8000) /* controller not enabled */
		{
			PADBarrelEnabled[chan] = 1; //if wavebird disconnects it cant reconnect
			u32 psize = sizeof(PADStatus)-1; //dont set error twice
			u8 *CurPad = (u8*)(&Pad[chan]);
			while(psize--) *CurPad++ = 0;
			if(HIDPad == HID_PAD_NOT_SET)
			{
				*HIDMotor = (MotorCommand[chan]&0x3);
				HIDPad = chan;
			}
			continue;
		}
		used |= (1<<chan);

		/* save IsBarrel status */
		PADIsBarrel[chan] = ((Pad[chan].button & 0x80) == 0) && PADBarrelEnabled[chan];
		if(PADIsBarrel[chan])
		{
			u8 curchan = chan*4;
			if(Pad[chan].button & (PAD_BUTTON_Y | PAD_BUTTON_B)) //left
			{
				if(PADBarrelPress[0+curchan] == 5)
					Pad[chan].button &= ~(PAD_BUTTON_Y | PAD_BUTTON_B);
				else
					PADBarrelPress[0+curchan]++;
			}
			else
				PADBarrelPress[0+curchan] = 0;

			if(Pad[chan].button & (PAD_BUTTON_X | PAD_BUTTON_A)) //right
			{
				if(PADBarrelPress[1+curchan] == 5)
					Pad[chan].button &= ~(PAD_BUTTON_X | PAD_BUTTON_A);
				else
					PADBarrelPress[1+curchan]++;
			}
			else
				PADBarrelPress[1+curchan] = 0;

			if(Pad[chan].button & PAD_BUTTON_START) //start
			{
				if(PADBarrelPress[2+curchan] == 5)
					Pad[chan].button &= ~PAD_BUTTON_START;
				else
					PADBarrelPress[2+curchan]++;
			}
			else
				PADBarrelPress[2+curchan] = 0;

			//signal lengthener
			if(PADBarrelPress[3+curchan] == 0)
			{
				u8 tmp_triggerR = ((PADTriggerCStick>>0)&0xFF);
				if(tmp_triggerR > 0x16) // need to do this manually
					PADBarrelPress[3+curchan] = 3;
			}
			else
			{
				Pad[chan].button |= PAD_TRIGGER_R;
				PADBarrelPress[3+curchan]--;
			}
		}
		else
		{
			if(Pad[chan].button & 0x80)
				Rumble |= ((1<<31)>>chan);
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
		}

		/* shutdown by pressing B,Z,R,PAD_BUTTON_DOWN */
		if((Pad[chan].button&0x234) == 0x234)
		{
			goto Shutdown;
		}
		if((Pad[chan].button&0x1030) == 0x1030)	//reset by pressing start, Z, R
		{
			/* reset status 3 */
			*reset = 0x3DEA;
		}
		else /* for held status */
			*reset = 0;
		/* clear unneeded button attributes */
		Pad[chan].button &= 0x9F7F;
		/* set current command */
		_siReg[chan*3] = (MotorCommand[chan]&0x3) | 0x00400300;
		/* transfer command */
		_siReg[14] |= (1<<31);
		while(_siReg[14] & (1<<31));
	}

	if (HIDPad == HID_PAD_NOT_SET)
		HIDPad = MaxPads;
	for (chan = HIDPad; (chan < HID_PAD_NONE); chan = HID_PAD_NONE) // Run once for now
	{
		if (HID_CTRL->MultiIn == 2)		//multiple controllers connected to a single usb port
		{
			used |= (1<<(PrevAdapterChannel1 + chan)) | (1<<(PrevAdapterChannel2 + chan)) | (1<<(PrevAdapterChannel3 + chan))| (1<<(PrevAdapterChannel4 + chan));	//depending on adapter it may only send every 4th time
			chan = chan + HID_Packet[0] - 1;	// the controller number is in the first byte 
			if (chan >= NIN_CFG_MAXPAD)		//if would be higher than the maxnumber of controllers
				continue;	//toss it and try next usb port
			PrevAdapterChannel1 = PrevAdapterChannel2;
			PrevAdapterChannel2 = PrevAdapterChannel3;
			PrevAdapterChannel3 = PrevAdapterChannel4;
			PrevAdapterChannel4 = HID_Packet[0] - 1;
		}
		memInvalidate = (u32)HID_Packet;
		asm volatile("dcbi 0,%0; sync; isync" : : "b"(memInvalidate) : "memory");

		if(HID_CTRL->Power.Mask &&	//shutdown if power configured and all power buttons pressed
		((HID_Packet[HID_CTRL->Power.Offset] & HID_CTRL->Power.Mask) == HID_CTRL->Power.Mask))
		{
			goto Shutdown;
		}
		used |= (1<<chan);

		Rumble |= ((1<<31)>>chan);
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
			if(((HID_Packet[HID_CTRL->Up.Offset] & HID_CTRL->DPADMask) == HID_CTRL->Up.Mask)		 || ((HID_Packet[HID_CTRL->UpLeft.Offset] & HID_CTRL->DPADMask) == HID_CTRL->UpLeft.Mask)			|| ((HID_Packet[HID_CTRL->RightUp.Offset]	& HID_CTRL->DPADMask) == HID_CTRL->RightUp.Mask))
				button |= PAD_BUTTON_UP;
	
			if(((HID_Packet[HID_CTRL->Right.Offset] & HID_CTRL->DPADMask) == HID_CTRL->Right.Mask) || ((HID_Packet[HID_CTRL->DownRight.Offset] & HID_CTRL->DPADMask) == HID_CTRL->DownRight.Mask)	|| ((HID_Packet[HID_CTRL->RightUp.Offset] & HID_CTRL->DPADMask) == HID_CTRL->RightUp.Mask))
				button |= PAD_BUTTON_RIGHT;
	
			if(((HID_Packet[HID_CTRL->Down.Offset] & HID_CTRL->DPADMask) == HID_CTRL->Down.Mask)	 || ((HID_Packet[HID_CTRL->DownRight.Offset] & HID_CTRL->DPADMask) == HID_CTRL->DownRight.Mask)	|| ((HID_Packet[HID_CTRL->DownLeft.Offset] & HID_CTRL->DPADMask) == HID_CTRL->DownLeft.Mask))
				button |= PAD_BUTTON_DOWN;
	
			if(((HID_Packet[HID_CTRL->Left.Offset] & HID_CTRL->DPADMask) == HID_CTRL->Left.Mask)	 || ((HID_Packet[HID_CTRL->DownLeft.Offset] & HID_CTRL->DPADMask) == HID_CTRL->DownLeft.Mask)		|| ((HID_Packet[HID_CTRL->UpLeft.Offset] & HID_CTRL->DPADMask) == HID_CTRL->UpLeft.Mask))
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
		if( HID_CTRL->DigitalLR == 2)	//no digital trigger buttons compute from analog trigger values
		{
			if ((HID_CTRL->VID == 0x0925) && (HID_CTRL->PID == 0x03E8))	//Mayflash Classic Controller Pro Adapter
			{
				if((HID_Packet[HID_CTRL->L.Offset] & 0x7C) >= HID_CTRL->L.Mask)	//only some bits are part of this control
					button |= PAD_TRIGGER_L;
				if((HID_Packet[HID_CTRL->R.Offset] & 0x0F) >= HID_CTRL->R.Mask)	//only some bits are part of this control
					button |= PAD_TRIGGER_R;
			}
			else	//standard no digital trigger button
			{
				if(HID_Packet[HID_CTRL->L.Offset] >= HID_CTRL->L.Mask)
					button |= PAD_TRIGGER_L;
				if(HID_Packet[HID_CTRL->R.Offset] >= HID_CTRL->R.Mask)
					button |= PAD_TRIGGER_R;
			}
		}
		else	//standard digital left and right trigger buttons
		{
			if(HID_Packet[HID_CTRL->L.Offset] & HID_CTRL->L.Mask)
				button |= PAD_TRIGGER_L;
			if(HID_Packet[HID_CTRL->R.Offset] & HID_CTRL->R.Mask)
				button |= PAD_TRIGGER_R;
		}
		if(HID_Packet[HID_CTRL->S.Offset] & HID_CTRL->S.Mask)
			button |= PAD_BUTTON_START;
		Pad[chan].button = button;

		if((Pad[chan].button&0x1030) == 0x1030)	//reset by pressing start, Z, R
		{
			/* reset status 3 */
			*reset = 0x3DEA;
		}
		else /* for held status */
			*reset = 0;

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
		else
		if ((HID_CTRL->VID == 0x045E) && (HID_CTRL->PID == 0x001B))	//Microsoft Sidewinder Force Feedback 2 Joystick
		{
			stickX		= ((HID_Packet[HID_CTRL->StickX.Offset] & 0xFC) >> 2) | ((HID_Packet[2] & 0x03) << 6);			//raw 80 81...FF 00 ... 7E 7F (left...center...right)
			stickY		= -1 - (((HID_Packet[HID_CTRL->StickY.Offset] & 0xFC) >> 2) | ((HID_Packet[4] & 0x03) << 6));	//raw 80 81...FF 00 ... 7E 7F (up...center...down)
			substickX	= HID_Packet[HID_CTRL->CStickX.Offset] * 4;			//raw E0 E1...FF 00 ... 1E 1F (left...center...right)
			substickY	= 127 - (HID_Packet[HID_CTRL->CStickY.Offset] * 2);	//raw 00 01...3F 40 ... 7E 7F (up...center...down)
		}
		else
		if ((HID_CTRL->VID == 0x044F) && (HID_CTRL->PID == 0xB315))	//Thrustmaster Dual Analog 4
		{
			stickX		= HID_Packet[HID_CTRL->StickX.Offset];			//raw 80 81...FF 00 ... 7E 7F (left...center...right)
			stickY		= -1 - HID_Packet[HID_CTRL->StickY.Offset];		//raw 80 81...FF 00 ... 7E 7F (up...center...down)
			substickX	= HID_Packet[HID_CTRL->CStickX.Offset];			//raw 80 81...FF 00 ... 7E 7F (left...center...right)
			substickY	= 127 - HID_Packet[HID_CTRL->CStickY.Offset];	//raw 00 01...7F 80 ... FE FF (up...center...down)
		}
		else
		if ((HID_CTRL->VID == 0x0925) && (HID_CTRL->PID == 0x03E8))	//Mayflash Classic Controller Pro Adapter
		{
			stickX		= ((HID_Packet[HID_CTRL->StickX.Offset] & 0x3F) << 2) - 128;	//raw 06 07 ... 1E 1F 20 ... 37 38 (left ... center ... right)
			stickY		= 127 - ((((HID_Packet[HID_CTRL->StickY.Offset] & 0x0F) << 2) | ((HID_Packet[3] & 0xC0) >> 6)) << 2);	//raw 06 07 ... 1F 20 21 ... 38 39 (up, center, down)
			substickX	= ((HID_Packet[HID_CTRL->CStickX.Offset] & 0x1F) << 3) - 128;	//raw 03 04 ... 0E 0F 10 ... 1B 1C (left ... center ... right)
			substickY	= 127 - ((((HID_Packet[HID_CTRL->CStickY.Offset] & 0x03) << 3) | ((HID_Packet[5] & 0xE0) >> 5)) << 3);	//raw 03 04 ... 1F 10 11 ... 1C 1D (up, center, down)
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
		if( HID_CTRL->DigitalLR == 1)
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
			else
			if ((HID_CTRL->VID == 0x0925) && (HID_CTRL->PID == 0x03E8))	//Mayflash Classic Controller Pro Adapter
			{
				tmp_triggerL =   ((HID_Packet[HID_CTRL->LAnalog] & 0x7C) >> 2) << 3;	//raw 04 ... 1F (out ... in)
				tmp_triggerR = (((HID_Packet[HID_CTRL->RAnalog] & 0x0F) << 1) | ((HID_Packet[6] & 0x80) >> 7)) << 3;	//raw 03 ... 1F (out ... in)
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

	if(MaxPads == 0) //wiiu
		MaxPads = 4;

	for(chan = 0; chan < MaxPads; ++chan)
	{
		if(used & (1<<chan))
		{
			BTPadFree[chan] = 0;
			continue;
		}
		BTPadFree[chan] = 1;

		memInvalidate = (u32)&BTPad[chan];
		asm volatile("dcbi 0,%0; sync ; isync" : : "b"(memInvalidate) : "memory");

		if(BTPad[chan].used == C_NOT_SET)
			continue;

		if(BTPad[chan].button & BT_BUTTON_HOME)
			goto Shutdown;

		used |= (1<<chan);

		Rumble |= ((1<<31)>>chan);
		BTMotor[chan] = MotorCommand[chan]&0x3;

		s8 tmp_stick = 0;
		if(BTPad[chan].xAxisL > 0x7F)
			tmp_stick = 0x7F;
		else if(BTPad[chan].xAxisL < -0x80)
			tmp_stick = -0x80;
		else
			tmp_stick = BTPad[chan].xAxisL;
		Pad[chan].stickX = tmp_stick;

		if(BTPad[chan].xAxisR > 0x7F)
			tmp_stick = 0x7F;
		else if(BTPad[chan].xAxisR < -0x80)
			tmp_stick = -0x80;
		else
			tmp_stick = BTPad[chan].xAxisR;
		Pad[chan].substickX = tmp_stick;

		if(BTPad[chan].yAxisL > 0x7F)
			tmp_stick = 0x7F;
		else if(BTPad[chan].yAxisL < -0x80)
			tmp_stick = -0x80;
		else
			tmp_stick = BTPad[chan].yAxisL;
		Pad[chan].stickY = tmp_stick;

		if(BTPad[chan].yAxisR > 0x7F)
			tmp_stick = 0x7F;
		else if(BTPad[chan].yAxisR < -0x80)
			tmp_stick = -0x80;
		else
			tmp_stick = BTPad[chan].yAxisR;
		Pad[chan].substickY = tmp_stick;

		u16 button = 0;
		if(BTPad[chan].button & BT_DPAD_LEFT)
			button |= PAD_BUTTON_LEFT;
		if(BTPad[chan].button & BT_DPAD_RIGHT)
			button |= PAD_BUTTON_RIGHT;
		if(BTPad[chan].button & BT_DPAD_DOWN)
			button |= PAD_BUTTON_DOWN;
		if(BTPad[chan].button & BT_DPAD_UP)
			button |= PAD_BUTTON_UP;

		if(BTPad[chan].used & C_CC)
		{
			Pad[chan].triggerLeft = BTPad[chan].triggerL;
			if(BTPad[chan].button & BT_TRIGGER_L)
				button |= PAD_TRIGGER_L;

			Pad[chan].triggerRight = BTPad[chan].triggerR;
			if(BTPad[chan].button & BT_TRIGGER_R)
				button |= PAD_TRIGGER_R;

			if(BTPad[chan].button & BT_TRIGGER_ZR)
				button |= PAD_TRIGGER_Z;
		}
		else
		{
			if(BTPad[chan].button & BT_TRIGGER_ZL)
			{
				if(BTPad[chan].button & BT_TRIGGER_L)
					Pad[chan].triggerLeft = 0x7F;
				else
				{
					button |= PAD_TRIGGER_L;
					Pad[chan].triggerLeft = 0xFF;
				}
			}
			else
				Pad[chan].triggerLeft = 0;

			if(BTPad[chan].button & BT_TRIGGER_ZR)
			{
				if(BTPad[chan].button & BT_TRIGGER_L)
					Pad[chan].triggerRight = 0x7F;
				else
				{
					button |= PAD_TRIGGER_R;
					Pad[chan].triggerRight = 0xFF;
				}
			}
			else
				Pad[chan].triggerRight = 0;

			if(BTPad[chan].button & BT_TRIGGER_R)
				button |= PAD_TRIGGER_Z;
		}

		if(BTPad[chan].used & C_SWAP)
		{	/* turn buttons quarter clockwise */
			if(BTPad[chan].button & BT_BUTTON_B)
				button |= PAD_BUTTON_A;
			if(BTPad[chan].button & BT_BUTTON_Y)
				button |= PAD_BUTTON_B;
			if(BTPad[chan].button & BT_BUTTON_A)
				button |= PAD_BUTTON_X;
			if(BTPad[chan].button & BT_BUTTON_X)
				button |= PAD_BUTTON_Y;
		}
		else
		{
			if(BTPad[chan].button & BT_BUTTON_A)
				button |= PAD_BUTTON_A;
			if(BTPad[chan].button & BT_BUTTON_B)
				button |= PAD_BUTTON_B;
			if(BTPad[chan].button & BT_BUTTON_X)
				button |= PAD_BUTTON_X;
			if(BTPad[chan].button & BT_BUTTON_Y)
				button |= PAD_BUTTON_Y;
		}
		if(BTPad[chan].button & BT_BUTTON_START)
			button |= PAD_BUTTON_START;

		Pad[chan].button = button;

		/* shutdown by pressing B,Z,R,PAD_BUTTON_DOWN */
		if((Pad[chan].button&0x234) == 0x234)
		{
			goto Shutdown;
		}
		if((Pad[chan].button&0x1030) == 0x1030)	//reset by pressing start, Z, R
		{
			/* reset status 3 */
			*reset = 0x3DEA;
		}
		else /* for held status */
			*reset = 0;
	}
	//some games need all pads without errors to work
	for(chan = 0; chan < MaxPads; ++chan)
		Pad[chan].err = (used & (1<<chan)) ? 0 : -1;

	*PadUsed = used;

	memFlush = (u32)HIDMotor;
	asm volatile("dcbf 0,%0; sync; isync" : : "b"(memFlush) : "memory");
	memFlush = (u32)BTMotor;
	asm volatile("dcbf 0,%0; sync; isync" : : "b"(memFlush) : "memory");

	return Rumble;

Shutdown:
	/* stop audio dma */
	_dspReg[27] = (_dspReg[27]&~0x8000);
	/* reset status 1 */
	*reset = 0x1DEA;
	while(*reset == 0x1DEA) ;
	/* load in stub */
	memFlush = (u32)stubdest;
	u32 end = memFlush + stubsize;
	for ( ; memFlush < end; memFlush += 32)
	{
		memInvalidate = (u32)stubsrc;
		asm volatile("dcbi 0,%0; sync; isync" : : "b"(memInvalidate) : "memory");
		u8 b;
		for(b = 0; b < 8; ++b)
			*stubdest++ = *stubsrc++;
		asm volatile("dcbst 0,%0; sync ; icbi 0,%0; isync" : : "b"(memFlush));
	}
	asm volatile(
		"sync; isync\n"
		"lis %r3, 0x8133\n"
		"mtlr %r3\n"
		"blr\n"
	);
	return 0;
}
