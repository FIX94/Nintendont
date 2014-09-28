/*
Stream.c for Nintendont (Kernel)

Copyright (C) 2014 FIX94

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

#include "Stream.h"
#include "string.h"
#include "adp.h"
#include "DI.h"
extern int dbgprintf( const char *fmt, ...);
u32 StreamEnd = 0;
u32 StreamEndOffset = 0;
s32 StreamSize = 0;
u32 StreamStart = 0;
u32 StreamCurrent = 0;

u8 *StreamBuffer = (u8*)0x132A0000;
#define REAL_STREAMING DIP_DMA_ADR
#define AI_ADP_LOC DIP_DMA_LEN

#define BUFSIZE 0xC400
#define CHUNK_48 0x3800
#define CHUNK_48to32 0x5400
u32 cur_chunksize = 0;

const u32 buf1 = 0x13280000;
const u32 buf2 = 0x1328C400;
u32 cur_buf = 0;
u32 buf_loc = 0;

s16 outl[SAMPLES_PER_BLOCK], outr[SAMPLES_PER_BLOCK];
long hist[4];

u32 samplecounter = 0;
s16 samplebufferL = 0, samplebufferR = 0;
PCMWriter CurrentWriter;
void StreamInit()
{
	memset(outl, 0, sizeof(outl));
	sync_after_write(outl, sizeof(outl));
	memset(outr, 0, sizeof(outr));
	sync_after_write(outl, sizeof(outl));

	memset32((void*)buf1, 0, BUFSIZE);
	sync_after_write((void*)buf1, BUFSIZE);
	memset32((void*)buf2, 0, BUFSIZE);
	sync_after_write((void*)buf2, BUFSIZE);

	write32(REAL_STREAMING, 0);
	write32(AI_ADP_LOC, 0);
}

void StreamPrepare(bool resample)
{
	memset32(hist, 0, sizeof(hist));
	if(resample)
	{
		//dbgprintf("Using 48kHz ADP -> 32kHz PCM Decoder\n");
		CurrentWriter = WritePCM48to32;
		cur_chunksize = CHUNK_48to32;
		samplecounter = 0;
		samplebufferL = 0;
		samplebufferR = 0;
	}
	else
	{
		//dbgprintf("Using 48kHz ADP -> 48kHz PCM Decoder\n");
		CurrentWriter = WritePCM48;
		cur_chunksize = CHUNK_48;
	}
}

static inline void write16INC(u32 buf, u32 *loc, s16 val)
{
	write16(buf+(*loc), val);
	*loc += 2;
}
static inline void SaveSamples(const u32 j, s16 *bufl, s16 *bufr)
{
	samplebufferL = bufl[j];
	samplebufferR = bufr[j];
}
static inline void CombineSamples(const u32 j, s16 *bufl, s16 *bufr)
{	/* average samples to keep quality about the same */
	bufl[j] = (bufl[j] + samplebufferL)>>1;
	bufr[j] = (bufr[j] + samplebufferR)>>1;
}
void StreamUpdate()
{
	buf_loc = 0;
	u32 i;
	for(i = 0; i < cur_chunksize; i += ONE_BLOCK_SIZE)
	{
		ADPdecodebuffer(StreamBuffer+i,outl,outr,&hist[0],&hist[1],&hist[2],&hist[3]);
		CurrentWriter();
	}
	sync_after_write((void*)cur_buf, BUFSIZE);
	cur_buf = (cur_buf == buf1) ? buf2 : buf1;
}

void WritePCM48to32()
{
	unsigned int j;
	for(j = 0; j < SAMPLES_PER_BLOCK; j++)
	{
		samplecounter++;
		if(samplecounter == 2)
		{
			SaveSamples(j, outl, outr);
			continue;
		}
		if(samplecounter == 3)
		{
			CombineSamples(j, outl, outr);
			samplecounter = 0;
		}
		write16INC(cur_buf, &buf_loc, outl[j]);
		write16INC(cur_buf, &buf_loc, outr[j]);
	}
}

void WritePCM48()
{
	unsigned int j;
	for(j = 0; j < SAMPLES_PER_BLOCK; j++)
	{
		write16INC(cur_buf, &buf_loc, outl[j]);
		write16INC(cur_buf, &buf_loc, outr[j]);
	}
}

u32 StreamGetChunkSize()
{
	return cur_chunksize;
}

#define STREAM_BASE 0x13026520
#define STREAM_UPDATE 0x13026524
#define FAKE_STREAMING 0x13026540
#define STREAM_LENGTH 0x13026544
#define STREAM_START 0x13026548
#define STREAM_CURRENT 0x1302654C
#define REALSTREAM_END 0x13026550

#define LOOP_ENABLED 0x13026580

extern s32 DI_Handle;
void StreamUpdateRegisters()
{
	sync_before_read( (void*)STREAM_BASE, 0x40 );
	if(read32(STREAM_BASE))		//decoder update, it WILL update
	{
		if(StreamEnd)
		{
			StreamEnd = 0;
			StreamCurrent = 0;
			write32(STREAM_BASE, 0); //clear status
			write32(REAL_STREAMING, 0); //stop playing
			write32(REALSTREAM_END, 1); //stream end
			sync_after_write( (void*)STREAM_BASE, 0x40 );
		}
		else if(StreamCurrent > 0)
		{
			write32(STREAM_BASE, 0); //clear status
			sync_after_write( (void*)STREAM_BASE, 0x20 );
			IOS_Ioctl(DI_Handle, 1, (void*)StreamCurrent, 0, (void*)StreamBuffer, StreamGetChunkSize());
			StreamCurrent += StreamGetChunkSize();
			write32(STREAM_CURRENT, StreamCurrent);
			if(StreamCurrent >= StreamEndOffset) //terrible loop but it works
			{
				u32 diff = StreamCurrent - StreamEndOffset;
				u32 startpos = StreamGetChunkSize() - diff;
				memset32(StreamBuffer + startpos, 0, diff);
				sync_before_read( (void*)LOOP_ENABLED, 0x20 );
				if(read32(LOOP_ENABLED) == 1)
				{
					StreamCurrent = StreamStart;
					StreamPrepare(true);
				}
				else
					StreamEnd = 1;
			}
			StreamUpdate();
		}
	}
	if(read32(STREAM_UPDATE))		//stream update, it WILL update
	{
		write32(STREAM_UPDATE, 0);
		write32(STREAM_BASE, 0); //clear buffer request
		sync_after_write( (void*)STREAM_BASE, 0x20 );
		u32 CurrentSize = read32(STREAM_LENGTH);
		u32 CurrentStart = read32(STREAM_START);
		write32(REALSTREAM_END, 0); //stream runs
		sync_after_write( (void*)FAKE_STREAMING, 0x20 );
		if(CurrentSize > 0 && CurrentStart > 0)
		{
			StreamSize = CurrentSize;
			StreamStart = CurrentStart;
			StreamCurrent = StreamStart;
			StreamEndOffset = StreamStart + StreamSize;
			StreamPrepare(true); //TODO: find a game which doesnt need resampling
			IOS_Ioctl(DI_Handle, 1, (void*)StreamCurrent, 0, (void*)StreamBuffer, StreamGetChunkSize());
			StreamCurrent += StreamGetChunkSize();
			cur_buf = buf1; //reset adp buffer
			StreamUpdate();
			/* Directly read in the second buffer */
			IOS_Ioctl(DI_Handle, 1, (void*)StreamCurrent, 0, (void*)StreamBuffer, StreamGetChunkSize());
			StreamCurrent += StreamGetChunkSize();
			StreamUpdate();
			/* Send stream signal to PPC */
			write32(STREAM_CURRENT, StreamCurrent);
			write32(AI_ADP_LOC, 0); //reset adp read pos
			write32(REAL_STREAMING, 0x20); //set stream flag
			sync_after_write( (void*)STREAM_BASE, 0x20 );
			//dbgprintf("Streaming %08x %08x\n", StreamStart, StreamSize);
			StreamEnd = 0;
		}
	}
	else if(read32(FAKE_STREAMING) == 0 && StreamCurrent > 0)		//stop stream
	{
		StreamCurrent = 0;
		write32(REAL_STREAMING, 0); //clear stream flag
	}
}
