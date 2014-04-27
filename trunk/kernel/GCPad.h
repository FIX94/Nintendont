#ifndef __GCPAD_H__
#define __GCPAD_H__

typedef struct
{
	union
	{
		struct 
		{
			bool ErrorStatus		:1;
			bool ErrorLatch			:1;
			u32	 Reserved			:1;
			bool Start				:1;

			bool Y					:1;
			bool X					:1;
			bool B					:1;
			bool A					:1;

			u32  AlwaysSet			:1;
			bool R					:1;
			bool L					:1;
			bool Z					:1;

			bool Up					:1;
			bool Down				:1;
			bool Right				:1;
			bool Left				:1;

			s16  StickX				:8;
			s16  StickY				:8;
		};
		u32 Buttons;
	};
	union 
	{
		struct 
		{
			s16  CStickX;
			s16  CStickY;
			s16  LShoulder;
			s16  RShoulder;	
		};
		u32 Sticks;
	};
} GCPadStatus;

#endif
