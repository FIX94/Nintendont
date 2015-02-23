/*
controller_ini_to_array for Nintendont (Kernel)

Copyright (C) 2015 FIX94

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
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <dirent.h>

typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned char u8;
typedef signed char s8;
u32 RumbleType = 0;
u32 RumbleEnabled = 0;
u32 bEndpointAddressOut = 0;
u8 *RawRumbleDataOn = NULL;
u8 *RawRumbleDataOff = NULL;
u32 RawRumbleDataLen = 0;
u32 RumbleTransferLen = 0;
u32 RumbleTransfers = 0;

typedef struct Layout
{
	u32 Offset;
	u32 Mask;
} layout;

typedef struct StickLayout
{
	u32 	Offset;
	s8		DeadZone;
	u32		Radius;
} stickLayout;

typedef struct Controller
{
	u32 VID;
	u32 PID;
	u32 Polltype;
	u32 DPAD;
	u32 DPADMask;
	u32 DigitalLR;
	u32 MultiIn;
	u32 MultiInValue;

	layout Power;

	layout A;
	layout B;
	layout X;
	layout Y;
	layout ZL;
	layout Z;
	
	layout L;
	layout R;
	layout S;
	
	layout Left;
	layout Down;
	layout Right;
	layout Up;

	layout RightUp;
	layout DownRight;
	layout DownLeft;
	layout UpLeft;

	stickLayout StickX;
	stickLayout StickY;
	stickLayout CStickX;
	stickLayout CStickY;
	u32 LAnalog;
	u32 RAnalog;

} controller;

unsigned int atox( char *String )
{
	u32 val=1;
	u32 len=0;
	u32 i;

	while(val)
	{
		switch(String[len])
		{
			case 0x0a:
			case 0x0d:
			case 0x00:
			case ',':
				val = 0;
				len--;
				break;
		}
		len++;
	}

	for( i=0; i < len; ++i )
	{
		if( String[i] >= '0' && String[i] <='9' )
		{
			val |= (String[i]-'0') << (((len-1)-i) * 4);

		} else if( String[i] >= 'A' && String[i] <='Z' ) {

			val |= (String[i]-'7') << (((len-1)-i) * 4);

		} else if( String[i] >= 'a' && String[i] <='z' ) {

			val |= (String[i]-'W') << (((len-1)-i) * 4);
		}
	}

	return val;
}

u32 ConfigGetValue( char *Data, const char *EntryName, u32 Entry )
{
	char entryname[128];
	sprintf( entryname, "\n%s=", EntryName );

	char *str = strstr( Data, entryname );
	if( str == (char*)NULL )
	{
		printf("Entry:\"%s\" not found!\r\n", EntryName );
		return 0;
	}

	str += strlen(entryname); // Skip '='

	char *strEnd = strchr( str, 0x0A );

	if( Entry == 0 )
	{
		return atox(str);

	} else if ( Entry == 1 ) {

		str = strstr( str, "," );
		if( str == (char*)NULL || str > strEnd )
		{
			printf("No \",\" found in entry.\r\n");
			return 0;
		}

		str++; //Skip ,

		return atox(str);
	} else if ( Entry == 2 ) {

		str = strstr( str, "," );
		if( str == (char*)NULL || str > strEnd )
		{
			printf("No \",\" found in entry.\r\n");
			return 0;
		}

		str++; //Skip the first ,

		str = strstr( str, "," );
		if( str == (char*)NULL || str > strEnd )
		{
			printf("No \",\" found in entry.\r\n");
			return 0;
		}

		str++; //Skip the second ,

		return atox(str);
	} else if ( Entry == 3 ) {
		u32 i;
		for(i = 0; i < RawRumbleDataLen; ++i)
		{
			RawRumbleDataOn[i] = atox(str);
			str = strstr( str, "," )+1;
		}
	} else if ( Entry == 4 ) {
		u32 i;
		for(i = 0; i < RawRumbleDataLen; ++i)
		{
			RawRumbleDataOff[i] = atox(str);
			str = strstr( str, "," )+1;
		}
	}

	return 0;
}

u32 ConfigGetDecValue( char *Data, const char *EntryName, u32 Entry )
{
	char entryname[128];
	sprintf( entryname, "\n%s=", EntryName );

	char *str = strstr( Data, entryname );
	if( str == (char*)NULL )
	{
		printf("Entry:\"%s\" not found!\r\n", EntryName );
		return 0;
	}

	str += strlen(entryname); // Skip '='

	char *strEnd = strchr( str, 0x0A );

	if( Entry == 0 )
	{
		return atoi(str);

	} else if ( Entry == 1 ) {

		str = strstr( str, "," );
		if( str == (char*)NULL || str > strEnd )
		{
			printf("No \",\" found in entry.\r\n");
			return 0;
		}

		str++; //Skip ,

		return atoi(str);
	} else if ( Entry == 2 ) {

		str = strstr( str, "," );
		if( str == (char*)NULL  || str > strEnd )
		{
			printf("No \",\" found in entry.\r\n");
			return 0;
		}

		str++; //Skip the first ,

		str = strstr( str, "," );
		if( str == (char*)NULL  || str > strEnd )
		{
			printf("No \",\" found in entry.\r\n");
			return 0;
		}

		str++; //Skip the second ,

		return atoi(str);
	}

	return 0;
}

void fprintfarray(FILE *f, u8*buf, size_t size)
{
	if(size == 0) return;

	size_t i;
	fprintf(f, "{");
	for(i = 0; i < size; ++i)
	{
		if(i > 0) fprintf(f, ",");
		fprintf(f, " 0x%02x", buf[i]);
	}
	fprintf(f, " }");
}
int main(int argc, char *argv[])
{
	DIR *cd = opendir(".");
	if(cd == NULL) return -2;
	struct dirent *curp;
	while((curp = readdir(cd)) != NULL)
	{
		if(strstr(curp->d_name, ".ini") == NULL)
			continue;
		FILE *f = fopen(curp->d_name, "r");
		if(f == NULL)
			continue;
		fseek(f, 0, SEEK_END);
		size_t fsize = ftell(f);
		rewind(f);
		char *Data = malloc(fsize+1);
		fread(Data, 1, fsize, f);
		Data[fsize] = '\0';
		fclose(f);
		controller *HID_CTRL = (controller*)malloc(sizeof(controller));
		memset(HID_CTRL, 0, sizeof(controller));

		HID_CTRL->VID = ConfigGetValue( Data, "VID", 0 );
		HID_CTRL->PID = ConfigGetValue( Data, "PID", 0 );

		HID_CTRL->DPAD		= ConfigGetValue( Data, "DPAD", 0 );
		HID_CTRL->DigitalLR	= ConfigGetValue( Data, "DigitalLR", 0 );
		HID_CTRL->Polltype	= ConfigGetValue( Data, "Polltype", 0 );
		HID_CTRL->MultiIn	= ConfigGetValue( Data, "MultiIn", 0 );

		if( HID_CTRL->MultiIn )
		{
			HID_CTRL->MultiInValue= ConfigGetValue( Data, "MultiInValue", 0 );

			printf("HID:MultIn:%u\r\n", HID_CTRL->MultiIn );
			printf("HID:MultiInValue:%u\r\n", HID_CTRL->MultiInValue );
		}

		//if( HID_CTRL->Polltype == 0 )
		//	MemPacketSize = 128;
		//else
		//	MemPacketSize = wMaxPacketSize;

		if( HID_CTRL->DPAD > 1 )
		{
			printf("HID: %u is an invalid DPAD value\r\n", HID_CTRL->DPAD );
			free(Data);
			continue;
		}

		HID_CTRL->Power.Offset	= ConfigGetValue( Data, "Power", 0 );
		HID_CTRL->Power.Mask	= ConfigGetValue( Data, "Power", 1 );

		HID_CTRL->A.Offset	= ConfigGetValue( Data, "A", 0 );
		HID_CTRL->A.Mask	= ConfigGetValue( Data, "A", 1 );

		HID_CTRL->B.Offset	= ConfigGetValue( Data, "B", 0 );
		HID_CTRL->B.Mask	= ConfigGetValue( Data, "B", 1 );

		HID_CTRL->X.Offset	= ConfigGetValue( Data, "X", 0 );
		HID_CTRL->X.Mask	= ConfigGetValue( Data, "X", 1 );

		HID_CTRL->Y.Offset	= ConfigGetValue( Data, "Y", 0 );
		HID_CTRL->Y.Mask	= ConfigGetValue( Data, "Y", 1 );

		HID_CTRL->ZL.Offset	= ConfigGetValue( Data, "ZL", 0 );
		HID_CTRL->ZL.Mask	= ConfigGetValue( Data, "ZL", 1 );

		HID_CTRL->Z.Offset	= ConfigGetValue( Data, "Z", 0 );
		HID_CTRL->Z.Mask	= ConfigGetValue( Data, "Z", 1 );

		HID_CTRL->L.Offset	= ConfigGetValue( Data, "L", 0 );
		HID_CTRL->L.Mask	= ConfigGetValue( Data, "L", 1 );

		HID_CTRL->R.Offset	= ConfigGetValue( Data, "R", 0 );
		HID_CTRL->R.Mask	= ConfigGetValue( Data, "R", 1 );

		HID_CTRL->S.Offset	= ConfigGetValue( Data, "S", 0 );
		HID_CTRL->S.Mask	= ConfigGetValue( Data, "S", 1 );

		HID_CTRL->Left.Offset	= ConfigGetValue( Data, "Left", 0 );
		HID_CTRL->Left.Mask		= ConfigGetValue( Data, "Left", 1 );

		HID_CTRL->Down.Offset	= ConfigGetValue( Data, "Down", 0 );
		HID_CTRL->Down.Mask		= ConfigGetValue( Data, "Down", 1 );

		HID_CTRL->Right.Offset	= ConfigGetValue( Data, "Right", 0 );
		HID_CTRL->Right.Mask	= ConfigGetValue( Data, "Right", 1 );

		HID_CTRL->Up.Offset		= ConfigGetValue( Data, "Up", 0 );
		HID_CTRL->Up.Mask		= ConfigGetValue( Data, "Up", 1 );

		if( HID_CTRL->DPAD )
		{
			HID_CTRL->RightUp.Offset	= ConfigGetValue( Data, "RightUp", 0 );
			HID_CTRL->RightUp.Mask		= ConfigGetValue( Data, "RightUp", 1 );

			HID_CTRL->DownRight.Offset	= ConfigGetValue( Data, "DownRight", 0 );
			HID_CTRL->DownRight.Mask	= ConfigGetValue( Data, "DownRight", 1 );

			HID_CTRL->DownLeft.Offset	= ConfigGetValue( Data, "DownLeft", 0 );
			HID_CTRL->DownLeft.Mask		= ConfigGetValue( Data, "DownLeft", 1 );

			HID_CTRL->UpLeft.Offset		= ConfigGetValue( Data, "UpLeft", 0 );
			HID_CTRL->UpLeft.Mask		= ConfigGetValue( Data, "UpLeft", 1 );
		}

		if( HID_CTRL->DPAD  &&	//DPAD == 1 and all offsets the same
			HID_CTRL->Left.Offset == HID_CTRL->Down.Offset &&
			HID_CTRL->Left.Offset == HID_CTRL->Right.Offset &&
			HID_CTRL->Left.Offset == HID_CTRL->Up.Offset &&
			HID_CTRL->Left.Offset == HID_CTRL->RightUp.Offset &&
			HID_CTRL->Left.Offset == HID_CTRL->DownRight.Offset &&
			HID_CTRL->Left.Offset == HID_CTRL->DownLeft.Offset &&
			HID_CTRL->Left.Offset == HID_CTRL->UpLeft.Offset )
		{
			HID_CTRL->DPADMask = HID_CTRL->Left.Mask | HID_CTRL->Down.Mask | HID_CTRL->Right.Mask | HID_CTRL->Up.Mask
				| HID_CTRL->RightUp.Mask | HID_CTRL->DownRight.Mask | HID_CTRL->DownLeft.Mask | HID_CTRL->UpLeft.Mask;	//mask is all the used bits ored togather
			if ((HID_CTRL->DPADMask & 0xF0) == 0)	//if hi nibble isnt used
				HID_CTRL->DPADMask = 0x0F;			//use all bits in low nibble
			if ((HID_CTRL->DPADMask & 0x0F) == 0)	//if low nibble isnt used
				HID_CTRL->DPADMask = 0xF0;			//use all bits in hi nibble
		}
		else
			HID_CTRL->DPADMask = 0xFFFF;	//check all the bits

		HID_CTRL->StickX.Offset		= ConfigGetValue( Data, "StickX", 0 );
		HID_CTRL->StickX.DeadZone	= ConfigGetValue( Data, "StickX", 1 );
		HID_CTRL->StickX.Radius		= ConfigGetDecValue( Data, "StickX", 2 );
		if (HID_CTRL->StickX.Radius == 0)
			HID_CTRL->StickX.Radius = 80;
		HID_CTRL->StickX.Radius = (u64)HID_CTRL->StickX.Radius * 1280 / (128 - HID_CTRL->StickX.DeadZone);	//adjust for DeadZone
	//		printf("HID:StickX:  Offset=%3X Deadzone=%3X Radius=%d\r\n", HID_CTRL->StickX.Offset, HID_CTRL->StickX.DeadZone, HID_CTRL->StickX.Radius);

		HID_CTRL->StickY.Offset		= ConfigGetValue( Data, "StickY", 0 );
		HID_CTRL->StickY.DeadZone	= ConfigGetValue( Data, "StickY", 1 );
		HID_CTRL->StickY.Radius		= ConfigGetDecValue( Data, "StickY", 2 );
		if (HID_CTRL->StickY.Radius == 0)
			HID_CTRL->StickY.Radius = 80;
		HID_CTRL->StickY.Radius = (u64)HID_CTRL->StickY.Radius * 1280 / (128 - HID_CTRL->StickY.DeadZone);	//adjust for DeadZone
	//		printf("HID:StickY:  Offset=%3X Deadzone=%3X Radius=%d\r\n", HID_CTRL->StickY.Offset, HID_CTRL->StickY.DeadZone, HID_CTRL->StickY.Radius);

		HID_CTRL->CStickX.Offset	= ConfigGetValue( Data, "CStickX", 0 );
		HID_CTRL->CStickX.DeadZone	= ConfigGetValue( Data, "CStickX", 1 );
		HID_CTRL->CStickX.Radius	= ConfigGetDecValue( Data, "CStickX", 2 );
		if (HID_CTRL->CStickX.Radius == 0)
			HID_CTRL->CStickX.Radius = 80;
		HID_CTRL->CStickX.Radius = (u64)HID_CTRL->CStickX.Radius * 1280 / (128 - HID_CTRL->CStickX.DeadZone);	//adjust for DeadZone
	//		printf("HID:CStickX: Offset=%3X Deadzone=%3X Radius=%d\r\n", HID_CTRL->CStickX.Offset, HID_CTRL->CStickX.DeadZone, HID_CTRL->CStickX.Radius);

		HID_CTRL->CStickY.Offset	= ConfigGetValue( Data, "CStickY", 0 );
		HID_CTRL->CStickY.DeadZone	= ConfigGetValue( Data, "CStickY", 1 );
		HID_CTRL->CStickY.Radius	= ConfigGetDecValue( Data, "CStickY", 2 );
		if (HID_CTRL->CStickY.Radius == 0)
			HID_CTRL->CStickY.Radius = 80;
		HID_CTRL->CStickY.Radius = (u64)HID_CTRL->CStickY.Radius * 1280 / (128 - HID_CTRL->CStickY.DeadZone);	//adjust for DeadZone
	//		printf("HID:CStickY: Offset=%3X Deadzone=%3X Radius=%d\r\n", HID_CTRL->CStickY.Offset, HID_CTRL->CStickY.DeadZone, HID_CTRL->CStickY.Radius);

		HID_CTRL->LAnalog	= ConfigGetValue( Data, "LAnalog", 0 );
		HID_CTRL->RAnalog	= ConfigGetValue( Data, "RAnalog", 0 );

		FILE *array = fopen("array.txt", "a");
		fprintf(array,
			"\t{ 0x%04x, 0x%04x, %i, %i, 0x%x, %i, %i, %i,\n"
			"\t{0x%x, 0x%x}, {0x%x, 0x%x}, {0x%x, 0x%x}, {0x%x, 0x%x}, {0x%x, 0x%x}, {0x%x, 0x%x}, {0x%x, 0x%x}, {0x%x, 0x%x}, {0x%x, 0x%x}, {0x%x, 0x%x},\n"
			"\t{0x%x, 0x%x}, {0x%x, 0x%x}, {0x%x, 0x%x}, {0x%x, 0x%x}, {0x%x, 0x%x}, {0x%x, 0x%x}, {0x%x, 0x%x}, {0x%x, 0x%x}, \n"
			"\t{0x%x, 0x%x, 0x%x}, {0x%x, 0x%x, 0x%x}, {0x%x, 0x%x, 0x%x}, {0x%x, 0x%x, 0x%x}, 0x%x, 0x%x },\n",
			HID_CTRL->VID, HID_CTRL->PID, HID_CTRL->Polltype, HID_CTRL->DPAD, HID_CTRL->DPADMask, HID_CTRL->DigitalLR, HID_CTRL->MultiIn, HID_CTRL->MultiInValue, 
			HID_CTRL->Power.Offset ,HID_CTRL->Power.Mask, HID_CTRL->A.Offset ,HID_CTRL->A.Mask, HID_CTRL->B.Offset ,HID_CTRL->B.Mask, 
			HID_CTRL->X.Offset ,HID_CTRL->X.Mask, HID_CTRL->Y.Offset ,HID_CTRL->Y.Mask, HID_CTRL->ZL.Offset ,HID_CTRL->ZL.Mask, 
			HID_CTRL->Z.Offset ,HID_CTRL->Z.Mask, HID_CTRL->L.Offset ,HID_CTRL->L.Mask, HID_CTRL->R.Offset ,HID_CTRL->R.Mask, HID_CTRL->S.Offset ,HID_CTRL->S.Mask,
			HID_CTRL->Left.Offset ,HID_CTRL->Left.Mask, HID_CTRL->Down.Offset ,HID_CTRL->Down.Mask, HID_CTRL->Right.Offset ,HID_CTRL->Right.Mask, 
			HID_CTRL->Up.Offset ,HID_CTRL->Up.Mask, HID_CTRL->RightUp.Offset ,HID_CTRL->RightUp.Mask, HID_CTRL->DownRight.Offset ,HID_CTRL->DownRight.Mask, 
			HID_CTRL->DownLeft.Offset ,HID_CTRL->DownLeft.Mask, HID_CTRL->UpLeft.Offset ,HID_CTRL->UpLeft.Mask, 
			HID_CTRL->StickX.Offset, HID_CTRL->StickX.DeadZone, HID_CTRL->StickX.Radius, HID_CTRL->StickY.Offset, HID_CTRL->StickY.DeadZone, HID_CTRL->StickY.Radius, 
			HID_CTRL->CStickX.Offset, HID_CTRL->CStickX.DeadZone, HID_CTRL->CStickX.Radius, HID_CTRL->CStickY.Offset, HID_CTRL->CStickY.DeadZone, HID_CTRL->CStickY.Radius, 
			HID_CTRL->LAnalog, HID_CTRL->RAnalog);
		fclose(array);
		if(ConfigGetValue( Data, "Rumble", 0 ))
		{
			RawRumbleDataLen = ConfigGetValue( Data, "RumbleDataLen", 0 );
			if(RawRumbleDataLen > 0)
			{
				RumbleEnabled = 1;
				u32 DataAligned = (RawRumbleDataLen+31) & (~31);

				if(RawRumbleDataOn != NULL) free(RawRumbleDataOn);
				RawRumbleDataOn = (u8*)malloc(DataAligned);
				memset(RawRumbleDataOn, 0, DataAligned);
				ConfigGetValue( Data, "RumbleDataOn", 3 );

				if(RawRumbleDataOff != NULL) free(RawRumbleDataOff);
				RawRumbleDataOff = (u8*)malloc(DataAligned);
				memset(RawRumbleDataOff, 0, DataAligned);
				ConfigGetValue( Data, "RumbleDataOff", 4 );

				RumbleType = ConfigGetValue( Data, "RumbleType", 0 );
				RumbleTransferLen = ConfigGetValue( Data, "RumbleTransferLen", 0 );
				RumbleTransfers = ConfigGetValue( Data, "RumbleTransfers", 0 );

				/*
				typedef struct Rumble {
					u32 VID;
					u32 PID;

					u32 RumbleType;
					u32 RumbleDataLen;
					u32 RumbleTransfers;
					u32 RumbleTransferLen;
					u8 *RumbleDataOn;
					u8 *RumbleDataOff;
				} rumble;
				*/
				array = fopen("rarray.txt", "a");
				fprintf(array,
					"\t{ 0x%04x, 0x%04x, %i, %i, %i, %i, RumbleDataOn_%04x_%04x, RumbleDataOff_%04x_%04x },\n",
						HID_CTRL->VID, HID_CTRL->PID, RumbleType, RawRumbleDataLen, RumbleTransfers, RumbleTransferLen,
							HID_CTRL->VID, HID_CTRL->PID, HID_CTRL->VID, HID_CTRL->PID);
				fclose(array);
				size_t arraypos;
				array = fopen("rdata.txt", "a");

				fprintf(array, "u8 RumbleDataOn_%04x_%04x[%i] = {", HID_CTRL->VID, HID_CTRL->PID, RawRumbleDataLen);
				for(arraypos = 0; arraypos < RawRumbleDataLen; ++arraypos)
				{
					if(arraypos > 0) fprintf(array, ",");
					fprintf(array, " 0x%02x", RawRumbleDataOn[arraypos]);
				}
				fprintf(array, " };\n");

				fprintf(array, "u8 RumbleDataOff_%04x_%04x[%i] = {", HID_CTRL->VID, HID_CTRL->PID, RawRumbleDataLen);
				for(arraypos = 0; arraypos < RawRumbleDataLen; ++arraypos)
				{
					if(arraypos > 0) fprintf(array, ",");
					fprintf(array, " 0x%02x", RawRumbleDataOff[arraypos]);
				}
				fprintf(array, " };\n");

				fclose(array);
			}
		}
		free(Data);
	}
	closedir(cd);
	FILE *controller_h = fopen("../kernel/HID_controllers.h", "w");
	FILE *array = fopen("array.txt", "r");
	FILE *rdata = fopen("rdata.txt", "r");
	FILE *rarray = fopen("rarray.txt", "r");
	fprintf(controller_h, "//This File was Generated using controller_ini_to_array\n\n");
	fprintf(controller_h, "#ifndef __HID_CONTROLLER_H__\n#define __HID_CONTROLLER_H_\n\n");
	if(array)
	{
		fseek(array, 0, SEEK_END);
		size_t size = ftell(array);
		rewind(array);
		fprintf(controller_h, "controller DefControllers[] = {\n");
		char *buf = malloc(size+1);
		while(fgets(buf, size, array))
			fprintf(controller_h, buf);
		fclose(array);
		fprintf(controller_h, "};\n\n");
		free(buf);
	}
	if(rdata)
	{
		fseek(rdata, 0, SEEK_END);
		size_t size = ftell(rdata);
		rewind(rdata);
		char *buf = malloc(size+1);
		while(fgets(buf, size, rdata))
			fprintf(controller_h, buf);
		fclose(rdata);
		fprintf(controller_h, "\n");
		free(buf);
	}
	if(rarray)
	{
		fseek(rarray, 0, SEEK_END);
		size_t size = ftell(rarray);
		rewind(rarray);
		fprintf(controller_h, "rumble DefRumble[] = {\n");
		char *buf = malloc(size+1);
		while(fgets(buf, size, rarray))
			fprintf(controller_h, buf);
		fclose(rarray);
		fprintf(controller_h, "};\n\n");
		free(buf);
	}
	fprintf(controller_h, "#endif\n");
	fclose(controller_h);
	remove("array.txt");
	remove("rdata.txt");
	remove("rarray.txt");
	return 0;
}
