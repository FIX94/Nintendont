#ifndef __PS3CONTROLLER_H__
#define __PS3CONTROLLER_H__

#define SS_DATA_LEN        53 //Should be 49
#define SS_LEDS_RUMBLE_LEN 32
#define SS_VENDOR_ID       0x054C //Sony Corp.
#define SS_PRODUCT_ID      0x0268 //Sixaxis and DS3
#define	SS_HEAP_SIZE       4096
#define SS_DEV_MAX 7

struct SS_BUTTONS    //Big endian
{
	u8 left     : 1;
	u8 down     : 1;
	u8 right    : 1;
	u8 up       : 1;
	u8 start    : 1;
	u8 R3       : 1;
	u8 L3       : 1;
	u8 select   : 1;
	
	u8 square   : 1;
	u8 cross    : 1;
	u8 circle   : 1;
	u8 triangle : 1;
	u8 R1       : 1;
	u8 L1       : 1;
	u8 R2       : 1;
	u8 L2       : 1;
	 
    u8 not_used : 7;
    u8 PS       : 1;
};

struct SS_ANALOG
{
    u8 x;
    u8 y;
};

struct SS_DPAD_SENSITIVE
{
    u8 up;
    u8 right;
    u8 down;
    u8 left;
};

struct SS_SHOULDER_SENSITIVE
{
    u8 L2;
    u8 R2;
    u8 L1;
    u8 R1;
};

struct SS_BUTTON_SENSITIVE
{
    u8 triangle;
    u8 circle;
    u8 cross;
    u8 square;
};

struct SS_MOTION
{
    u16 accX;
    u16 accY;
    u16 accZ;
    u16 Zgyro;
};
struct SS_GAMEPAD
{
    u8                        HIDdata;
    u8                        unk0;
    struct SS_BUTTONS        buttons;
    u8                        unk1;
    struct SS_ANALOG               leftAnalog;
    struct SS_ANALOG               rightAnalog;
    u32                       unk2;
    struct SS_DPAD_SENSITIVE       dpad_sens;
    struct SS_SHOULDER_SENSITIVE   shoulder_sens;
    struct SS_BUTTON_SENSITIVE     button_sens;
    u16                       unk3;
    u8                        unk4;
    u8                        status;
    u8                        power_rating;
    u8                        comm_status;
    u32                       unk5;
    u32                       unk6;
    u8                        unk7;
    struct SS_MOTION               motion;
    u8                        padding[3];
}__attribute__((packed));

struct SickSaxis
{
    struct SS_GAMEPAD gamepad;
    u8  leds_rumble[SS_LEDS_RUMBLE_LEN];  
	u8 connected;
	
    s32 fd, dev_id;
};

#endif