#include "../../../common/include/CommonConfig.h"
#define PAD_CHAN0_BIT				0x80000000
typedef struct _padstatus {
	unsigned short button;
	char stickX;
	char stickY;
	char substickX;
	char substickY;
	unsigned char triggerLeft;
	unsigned char triggerRight;
	unsigned char analogA;
	unsigned char analogB;
	char err;
} PADStatus;

static unsigned int chan, MaxPads;
static volatile unsigned int PADButtonsStick, PADTriggerCStick;
static unsigned int stubsize = 0x10000;
static char *stubdest = (char*)0x80001800;
static char *stubsrc = (char*)0x92010010;
static volatile unsigned short* const _dspReg = (unsigned short*)0xCC005000;
static volatile unsigned int* const _siReg = (unsigned int*)0xCD006400;
static volatile unsigned int* const MotorCommand = (unsigned int*)0xD2003010;
static char *a;
unsigned int regs[29];
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
	PADStatus *Pad = (PADStatus*)(((char*)regs[5])-0x30); //r5=return, 0x30 buffer before it
	MaxPads = ((NIN_CFG*)0xD2002A18)->MaxPads;
	if ((MaxPads > NIN_CFG_MAXPAD) || (MaxPads == 0))
		MaxPads = NIN_CFG_MAXPAD;
	for (chan = 0; chan < MaxPads; ++chan)
	{
		/* transfer the actual data */
		unsigned int x;
		unsigned int addr = 0xCD006400 + (0x0c * chan);
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
			unsigned int psize = sizeof(PADStatus);
			unsigned char *CurPad = (unsigned char*)(&Pad[chan]);
			while(psize--) *CurPad++ = 0;
			Pad[chan].err = -1;
			continue;
		}
		Pad[chan].err = 0;
		Pad[chan].stickX = ((PADButtonsStick>>8)&0xFF)-128;
		Pad[chan].stickY = ((PADButtonsStick>>0)&0xFF)-128;
		Pad[chan].substickX = ((PADTriggerCStick>>24)&0xFF)-128;
		Pad[chan].substickY = ((PADTriggerCStick>>16)&0xFF)-128;
		unsigned char tmp_triggerL = ((PADTriggerCStick>>8)&0xFF);
		Pad[chan].triggerLeft = (tmp_triggerL > 0x24 ? tmp_triggerL : 0);	//apply DEADZONE
		unsigned char tmp_triggerR = ((PADTriggerCStick>>0)&0xFF);
		Pad[chan].triggerRight = (tmp_triggerR > 0x24 ? tmp_triggerR : 0);	//apply DEADZONE
		
		if((Pad[chan].button&0x234) == 0x234)	//shutdown by pressing B,Z,R,PAD_BUTTON_DOWN
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
		/* clear unneeded button attributes */
		Pad[chan].button &= 0x9F7F;
		/* set current command */
		_siReg[chan*3] = (MotorCommand[chan]&0x3) | 0x00400300;
	}
	/* transfer all commands */
	_siReg[14] = 0x80000000;
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
	return;
}