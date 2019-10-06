
#ifndef _GLOBAL_H_
#define _GLOBAL_H_

typedef volatile unsigned char vu8;
typedef volatile unsigned short vu16;
typedef volatile unsigned int vu32;
typedef volatile unsigned long long vu64;

typedef volatile signed char vs8;
typedef volatile signed short vs16;
typedef volatile signed int vs32;
typedef volatile signed long long vs64;

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long long s64;

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

struct BTPadCont {
	u32 used;
	s16 xAxisL;
	s16 xAxisR;
	s16 yAxisL;
	s16 yAxisR;
	u32 button;
	u8 triggerL;
	u8 triggerR;
	s16 xAccel;
	s16 yAccel;
	s16 zAccel;
} __attribute__((aligned(32)));

#define PAD_BUTTON_LEFT         0x0001
#define PAD_BUTTON_RIGHT        0x0002
#define PAD_BUTTON_DOWN         0x0004
#define PAD_BUTTON_UP           0x0008
#define PAD_TRIGGER_Z           0x0010
#define PAD_TRIGGER_R           0x0020
#define PAD_TRIGGER_L           0x0040
#define PAD_BUTTON_A            0x0100
#define PAD_BUTTON_B            0x0200
#define PAD_BUTTON_X            0x0400
#define PAD_BUTTON_Y            0x0800
#define PAD_BUTTON_MENU         0x1000
#define PAD_BUTTON_START        0x1000

#define BT_DPAD_UP              0x0001
#define BT_DPAD_LEFT            0x0002
#define BT_TRIGGER_ZR           0x0004
#define BT_BUTTON_X             0x0008
#define BT_BUTTON_A             0x0010
#define BT_BUTTON_Y             0x0020
#define BT_BUTTON_B             0x0040
#define BT_TRIGGER_ZL           0x0080
#define BT_TRIGGER_R            0x0200
#define BT_BUTTON_START         0x0400
#define BT_BUTTON_HOME          0x0800
#define BT_BUTTON_SELECT        0x1000
#define BT_TRIGGER_L            0x2000
#define BT_DPAD_DOWN            0x4000
#define BT_DPAD_RIGHT           0x8000

#define WM_BUTTON_TWO			0x0001
#define WM_BUTTON_ONE			0x0002
#define WM_BUTTON_B				0x0004
#define WM_BUTTON_A				0x0008
#define WM_BUTTON_MINUS			0x0010
#define NUN_BUTTON_Z			0x0020 
#define NUN_BUTTON_C			0x0040
#define WM_BUTTON_HOME			0x0080
#define WM_BUTTON_LEFT			0x0100
#define WM_BUTTON_RIGHT			0x0200
#define WM_BUTTON_DOWN			0x0400
#define WM_BUTTON_UP			0x0800
#define WM_BUTTON_PLUS			0x1000

#endif
