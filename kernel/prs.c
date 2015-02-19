#include "global.h"
#include "string.h"

/* Credits go to fuzziqer for writing this code */

u32 prs_decompress(void* source,void* dest) // 800F7CB0 through 800F7DE4 in mem 
{
	//u32 r0,r3,r5,r6,r9; // 6 unnamed registers
	u32 r3, r5; //actually used registers
	u32 bitpos = 9; // 4 named registers 
	u8* sourceptr = (u8*)source;
	//u8* sourceptr_orig = (u8*)source;
	u8* destptr = (u8*)dest;
	u8* destptr_orig = (u8*)dest;
	u8 currentbyte;
	bool flag;
	int offset;
	u32 x,t; // 2 placed variables 

	//printf("\n> decompressing\n");
	currentbyte = sourceptr[0];
	sourceptr++;
	for (;;)
	{
		bitpos--;
		if (bitpos == 0)
		{
			currentbyte = sourceptr[0];
			bitpos = 8;
			sourceptr++;
		}
		flag = currentbyte & 1;
		currentbyte = currentbyte >> 1;
		if (flag)
		{
			destptr[0] = sourceptr[0];
			//printf("> > > %08X->%08X byte\n",sourceptr - sourceptr_orig,destptr - destptr_orig);
			sourceptr++;
			destptr++;
			continue;
		}
		bitpos--;
		if (bitpos == 0)
		{
			currentbyte = sourceptr[0];
			bitpos = 8;
			sourceptr++;
		}
		flag = currentbyte & 1;
		currentbyte = currentbyte >> 1;
		if (flag)
		{
			r3 = sourceptr[0] & 0xFF;
			//printf("> > > > > first: %02X - ",sourceptr[0]); system("PAUSE");
			offset = ((sourceptr[1] & 0xFF) << 8) | r3;
			//printf("> > > > > second: %02X - ",sourceptr[1]); system("PAUSE");
			sourceptr += 2;
			if (offset == 0) return (u32)(destptr - destptr_orig);
			r3 = r3 & 0x00000007;
			r5 = (offset >> 3) | 0xFFFFE000;
			if (r3 == 0)
			{
				flag = 0;
				r3 = sourceptr[0] & 0xFF;
				sourceptr++;
				r3++;
			} else r3 += 2;
			r5 += (u32)destptr;
			//printf("> > > %08X->%08X ldat %08X %08X %s\n",sourceptr - sourceptr_orig,destptr - destptr_orig,r5 - (u32)destptr,r3,flag ? "inline" : "extended");
		} else {
			r3 = 0;
			for (x = 0; x < 2; x++)
			{
				bitpos--;
				if (bitpos == 0)
				{
					currentbyte = sourceptr[0];
					bitpos = 8;
					sourceptr++;
				}
				flag = currentbyte & 1;
				currentbyte = currentbyte >> 1;
				offset = r3 << 1;
				r3 = offset | flag;
			}
			offset = sourceptr[0] | 0xFFFFFF00;
			r3 += 2;
			sourceptr++;
			r5 = offset + (u32)destptr;
			//printf("> > > %08X->%08X sdat %08X %08X\n",sourceptr - sourceptr_orig,destptr - destptr_orig,r5 - (u32)destptr,r3);
		}
		if (r3 == 0) continue;
		t = r3;
		for (x = 0; x < t; x++)
		{
			destptr[0] = *(u8*)r5;
			r5++;
			r3++;
			destptr++;
		}
	}
}

u32 prs_decompress_size(void* source)
{
	//u32 r0,r3,r5,r6,r9; // 6 unnamed registers
	u32 r3, r5; //actually used registers
	u32 bitpos = 9; // 4 named registers
	u8* sourceptr = (u8*)source;
	u32 size = 0;
	u8 currentbyte;//,lastbyte;
	bool flag;
	int offset;
	u32 x,t; // 2 placed variables 

	//printf("> %08X -> %08X: begin\n",sourceptr,destptr);
	currentbyte = sourceptr[0];
	//printf("> [ ] %08X -> %02X: command stream\n",sourceptr,currentbyte);
	sourceptr++;
	for (;;)
	{
		bitpos--;
		if (bitpos == 0)
		{
			/*lastbyte = */currentbyte = sourceptr[0];
			bitpos = 8;
			//printf("> [ ] %08X -> %02X: command stream\n",sourceptr,currentbyte);
			sourceptr++;
		}
		flag = currentbyte & 1;
		currentbyte = currentbyte >> 1;
		if (flag)
		{
			//printf("> [1] %08X -> %08X: %02X\n",sourceptr,destptr,*sourceptr);
			sourceptr++;
			size++;
			continue;
		}
		//printf("> [0] extended\n");
		bitpos--;
		if (bitpos == 0)
		{
			/*lastbyte = */currentbyte = sourceptr[0];
			bitpos = 8;
			//printf("> [ ] %08X -> %02X: command stream\n",sourceptr,currentbyte);
			sourceptr++;
		}
		flag = currentbyte & 1;
		currentbyte = currentbyte >> 1;
		if (flag)
		{
			r3 = sourceptr[0];
			offset = (sourceptr[1] << 8) | r3;
			sourceptr += 2;
			if (offset == 0) return size;
			r3 = r3 & 0x00000007;
			r5 = (offset >> 3) | 0xFFFFE000;
			if (r3 == 0)
			{
				r3 = sourceptr[0];
				sourceptr++;
				r3++;
			} else r3 += 2;
			r5 += size;
			//printf("> > [1] %08X -> %08X: block copy (%d)\n",r5,destptr,r3);
		} else {
			r3 = 0;
			for (x = 0; x < 2; x++)
			{
				bitpos--;
				if (bitpos == 0)
				{
					/*lastbyte = */currentbyte = sourceptr[0];
					bitpos = 8;
					//printf("> [ ] %08X -> %02X: command stream\n",sourceptr,currentbyte);
					sourceptr++;
				}
				flag = currentbyte & 1;
				currentbyte = currentbyte >> 1;
				offset = r3 << 1;
				r3 = offset | flag;
			}
			offset = sourceptr[0] | 0xFFFFFF00;
			r3 += 2;
			sourceptr++;
			r5 = offset + size;
			//printf("> > [0] %08X -> %08X: block copy (%d)\n",r5,destptr,r3);
		}
		if (r3 == 0) continue;
		t = r3;
		//printf("> > [ ] copying %d bytes\n",t);
		for (x = 0; x < t; x++)
		{
			r5++;
			r3++;
			size++;
		}
	}
}

