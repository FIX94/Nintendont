/*
	Hardware routines for reading and writing to the Wii's internal
	SD slot.

 Copyright (c) 2008-2013
   Michael Wiedenbauer (shagkur)
   Dave Murphy (WinterMute)
   Sven Peter <svpe@gmx.net>
	
 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation and/or
     other materials provided with the distribution.
  3. The name of the author may not be used to endorse or promote products derived
     from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/
#include "SDI.h"
#include "vsprintf.h"
#include "alloc.h"

#ifndef DEBUG_SD
#define dbgprintf(...)
#else
extern int dbgprintf( const char *fmt, ...);
#endif

s32 SDHandle;
u16 SDRCA;
u8	SDCID[16];
u32 IsSDHC;
 
s32 __sd0_fd;
static u16 __sd0_rca;
static s32 __sd0_initialized;
static s32 __sd0_sdhc;
//static u8 __sd0_csd[16];
static u8 __sd0_cid[16];
 
static s32 __sdio_initialized;

static u8 *rw_buffer;
 
static char _sd0_fs[] = "/dev/sdio/slot0";

vector *iovec;
struct _sdiorequest *request;
struct _sdioresponse *response;

static s32 __sdio_sendcommand(u32 cmd,u32 cmd_type,u32 rsp_type,u32 arg,u32 blk_cnt,u32 blk_size,void *buffer,void *reply,u32 rlen)
{
	s32 ret;
	
	request->cmd = cmd;
	request->cmd_type = cmd_type;
	request->rsp_type = rsp_type;
	request->arg = arg;
	request->blk_cnt = blk_cnt;
	request->blk_size = blk_size;
	request->dma_addr = buffer;
	request->isdma = ((buffer!=NULL)?1:0);
	request->pad0 = 0;
 
	if(request->isdma || __sd0_sdhc == 1)
	{
		iovec[0].data = (u32)request;
		iovec[0].len = sizeof(struct _sdiorequest);
		iovec[1].data = (u32)buffer;
		iovec[1].len = (blk_size*blk_cnt);
		iovec[2].data = (u32)response;
		iovec[2].len = sizeof(struct _sdioresponse);
		ret = IOS_Ioctlv(__sd0_fd,IOCTL_SDIO_SENDCMD,2,1,iovec);
	} else
		ret = IOS_Ioctl(__sd0_fd,IOCTL_SDIO_SENDCMD,request,sizeof(struct _sdiorequest),response,sizeof(struct _sdioresponse));
 
	if(reply && !(rlen>16))
		memcpy(reply,response,rlen);

	return ret;
}
static s32 __sdio_setclock(u32 set)
{
	s32 ret;
	u32 *clock = (u32*)malloc( sizeof(u32) );

	*clock = set;
	ret = IOS_Ioctl(__sd0_fd,IOCTL_SDIO_SETCLK,clock,sizeof(u32),NULL,0);
 
	free(clock);
	return ret;
}
u32 *status = NULL;
s32 __sdio_getstatus()
{
	s32 ret;

	ret = IOS_Ioctl(__sd0_fd,IOCTL_SDIO_GETSTATUS,NULL,0,status,sizeof(u32));
	if(ret<0) return ret;
 
	return *status;
}
static s32 __sdio_resetcard()
{
	s32 ret;

	__sd0_rca = 0;
	ret = IOS_Ioctl(__sd0_fd,IOCTL_SDIO_RESETCARD,NULL,0,status,sizeof(u32));
	if(ret<0) return ret;
 
	__sd0_rca = (u16)(*status>>16);
	return (*status&0xffff);
}
static s32 __sdio_gethcr(u8 reg, u8 size, u32 *val)
{
	s32 ret;
	u32 *hcr_value = (u32*)malloc( sizeof(u32) );
	u32 *hcr_query = (u32*)malloc( sizeof(u32) * 6 );
 
	if(val==NULL) return -4;
 
 	*hcr_value = 0;
	*val = 0;
	hcr_query[0] = reg;
	hcr_query[1] = 0;
	hcr_query[2] = 0;
	hcr_query[3] = size;
	hcr_query[4] = 0;
	hcr_query[5] = 0;
	ret = IOS_Ioctl(__sd0_fd,IOCTL_SDIO_READHCREG,(void*)hcr_query,24,hcr_value,sizeof(u32));
	*val = *hcr_value;
	
	free( hcr_value );
	free( hcr_query );
 
	return ret;
}
static s32 __sdio_sethcr(u8 reg, u8 size, u32 data)
{
	s32 ret;
	u32 *hcr_query = (u32*)malloc( sizeof(u32) * 6 );
 
	hcr_query[0] = reg;
	hcr_query[1] = 0;
	hcr_query[2] = 0;
	hcr_query[3] = size;
	hcr_query[4] = data;
	hcr_query[5] = 0;
	ret = IOS_Ioctl(__sd0_fd,IOCTL_SDIO_WRITEHCREG,(void*)hcr_query,24,NULL,0);
	
	free( hcr_query );
 
	return ret;
}
static s32 __sdio_waithcr(u8 reg, u8 size, u8 unset, u32 mask)
{
	u32 val;
	s32 ret;
	s32 tries = 10;

	while(tries-- > 0)
	{
		ret = __sdio_gethcr(reg, size, &val);
		if(ret < 0) return ret;
		if((unset && !(val & mask)) || (!unset && (val & mask))) return 0;
		udelay(10000);
	}

	return -1;
}
static s32 __sdio_setbuswidth(u32 bus_width)
{
	s32 ret;
	u32 hc_reg = 0;
 
	ret = __sdio_gethcr(SDIOHCR_HOSTCONTROL, 1, &hc_reg);
	if(ret<0) return ret;
 
	hc_reg &= 0xff; 	
	hc_reg &= ~SDIOHCR_HOSTCONTROL_4BIT;
	if(bus_width==4) hc_reg |= SDIOHCR_HOSTCONTROL_4BIT;
 
	return __sdio_sethcr(SDIOHCR_HOSTCONTROL, 1, hc_reg);		
}
static s32 __sd0_getrca()
{
	s32 ret;
	u32 rca;
 
	ret = __sdio_sendcommand(SDIO_CMD_SENDRCA,0,SDIO_RESPONSE_R5,0,0,0,NULL,&rca,sizeof(rca));	
	if(ret<0) return ret;

	__sd0_rca = (u16)(rca>>16);
	return (rca&0xffff);
}
 static s32 __sd0_select()
{
	s32 ret;
 
	ret = __sdio_sendcommand(SDIO_CMD_SELECT,SDIOCMD_TYPE_AC,SDIO_RESPONSE_R1B,(__sd0_rca<<16),0,0,NULL,NULL,0);
 
	return ret;
}
 static s32 __sd0_deselect()
{
	s32 ret;
 
	ret = __sdio_sendcommand(SDIO_CMD_DESELECT,SDIOCMD_TYPE_AC,SDIO_RESPONSE_R1B,0,0,0,NULL,NULL,0);
 
	return ret;
}
 static s32 __sd0_setblocklength(u32 blk_len)
{
	s32 ret;
 
	ret = __sdio_sendcommand(SDIO_CMD_SETBLOCKLEN,SDIOCMD_TYPE_AC,SDIO_RESPONSE_R1,blk_len,0,0,NULL,NULL,0);
 
	return ret;
}
 static s32 __sd0_setbuswidth(u32 bus_width)
{
	u16 val;
	s32 ret;
 
	val = 0x0000;
	if(bus_width==4) val = 0x0002;
 
	ret = __sdio_sendcommand(SDIO_CMD_APPCMD,SDIOCMD_TYPE_AC,SDIO_RESPONSE_R1,(__sd0_rca<<16),0,0,NULL,NULL,0);
	if(ret<0) return ret;
 
	ret = __sdio_sendcommand(SDIO_ACMD_SETBUSWIDTH,SDIOCMD_TYPE_AC,SDIO_RESPONSE_R1,val,0,0,NULL,NULL,0);
 
	return ret;		
}
static s32 __sd0_getcid()
{
	s32 ret;
 
	ret = __sdio_sendcommand(SDIO_CMD_ALL_SENDCID,0,SDIO_RESPOSNE_R2,(__sd0_rca<<16),0,0,NULL,__sd0_cid,16);
 
	return ret;
}
s32 SDHCInit()
{
	s32 ret;
	s32 tries;
	status = (u32*)malloc(sizeof(u32));
	struct _sdioresponse resp;
	
	dbgprintf("SDHCInit()\n");

	__sd0_rca = 0;
	__sd0_initialized = 0;
	__sd0_sdhc = 0;
	__sdio_initialized = 0;

	rw_buffer = (u8*)malloc( 4 * 1024 );
	
	iovec = (vector*)malloc( sizeof(vector) * 3 );
	request = (struct _sdiorequest*)malloc( sizeof(struct _sdiorequest) );
	response = (struct _sdioresponse*)malloc( sizeof(struct _sdioresponse) );
	
	dbgprintf("SD:Heap:%X\n", rw_buffer );

	__sd0_fd = IOS_Open(_sd0_fs,1);
	if ( __sd0_fd < 0 )
	{
		dbgprintf("Failed to SD\n");
		return __sd0_fd;
	}

	__sdio_resetcard();
	__sdio_getstatus();
	
	if(!((*status)&SDIO_STATUS_CARD_INSERTED))
		return false;

	if(!((*status)&SDIO_STATUS_CARD_INITIALIZED))
	{
		// IOS doesn't like this card, so we need to convice it to accept it.

		// reopen the handle which makes IOS clean stuff up
		IOS_Close(__sd0_fd);
		__sd0_fd = IOS_Open(_sd0_fs,1);

		// reset the host controller
		if(__sdio_sethcr(SDIOHCR_SOFTWARERESET, 1, 7) < 0) goto fail;
		if(__sdio_waithcr(SDIOHCR_SOFTWARERESET, 1, 1, 7) < 0) goto fail;

		// initialize interrupts (sd_reset_card does this on success)
		__sdio_sethcr(0x34, 4, 0x13f00c3);
		__sdio_sethcr(0x38, 4, 0x13f00c3);

		// enable power
		__sd0_sdhc = 1;
		ret = __sdio_sethcr(SDIOHCR_POWERCONTROL, 1, 0xe);
		if(ret < 0) goto fail;
		ret = __sdio_sethcr(SDIOHCR_POWERCONTROL, 1, 0xf);
		if(ret < 0) goto fail;

		// enable internal clock, wait until it gets stable and enable sd clock
		ret = __sdio_sethcr(SDIOHCR_CLOCKCONTROL, 2, 0);
		if(ret < 0) goto fail;
		ret = __sdio_sethcr(SDIOHCR_CLOCKCONTROL, 2, 0x101);
		if(ret < 0) goto fail;
		ret = __sdio_waithcr(SDIOHCR_CLOCKCONTROL, 2, 0, 2);
		if(ret < 0) goto fail;
		ret = __sdio_sethcr(SDIOHCR_CLOCKCONTROL, 2, 0x107);
		if(ret < 0) goto fail;

		// setup timeout
		ret = __sdio_sethcr(SDIOHCR_TIMEOUTCONTROL, 1, SDIO_DEFAULT_TIMEOUT);
		if(ret < 0) goto fail;

		// standard SDHC initialization process
		ret = __sdio_sendcommand(SDIO_CMD_GOIDLE, 0, 0, 0, 0, 0, NULL, NULL, 0);
		if(ret < 0) goto fail;
		ret = __sdio_sendcommand(SDIO_CMD_SENDIFCOND, 0, SDIO_RESPONSE_R6, 0x1aa, 0, 0, NULL, &resp, sizeof(resp));
		if(ret < 0) goto fail;
		if((resp.rsp_fields[0] & 0xff) != 0xaa) goto fail;

		tries = 10;
		while(tries-- > 0)
		{
			ret = __sdio_sendcommand(SDIO_CMD_APPCMD, SDIOCMD_TYPE_AC,SDIO_RESPONSE_R1,0,0,0,NULL,NULL,0);
			if(ret < 0) goto fail;
			ret = __sdio_sendcommand(SDIO_ACMD_SENDOPCOND, 0, SDIO_RESPONSE_R3, 0x40300000, 0, 0, NULL, &resp, sizeof(resp));
			if(ret < 0) goto fail;
			if(resp.rsp_fields[0] & (1 << 31)) break;

			udelay(10000);
		}
		if(tries < 0) goto fail;

		// FIXME: SDv2 cards which are not high-capacity won't work :/
		if(resp.rsp_fields[0] & (1 << 30))
			__sd0_sdhc = 1;
		else
			__sd0_sdhc = 0;

		ret = __sd0_getcid();
		if(ret < 0) goto fail;
		ret = __sd0_getrca();
		if(ret < 0) goto fail;
	}
	else if((*status)&SDIO_STATUS_CARD_SDHC)
		__sd0_sdhc = 1;
	else
		__sd0_sdhc = 0;
 
	ret = __sdio_setbuswidth(4);
	if(ret<0) return false;
 
	ret = __sdio_setclock(1);
	if(ret<0) return false;
 
	ret = __sd0_select();
	if(ret<0) return false;
 
	ret = __sd0_setblocklength(PAGE_SIZE512);
	if(ret<0) {
		ret = __sd0_deselect();
		return false;
	}
 
	ret = __sd0_setbuswidth(4);
	if(ret<0) {
		ret = __sd0_deselect();
		return false;
	}
	__sd0_deselect();

	__sd0_initialized = 1;
	return true;

fail:
	dbgprintf("SDInit failed\n");
	__sdio_sethcr(SDIOHCR_SOFTWARERESET, 1, 7);
	__sdio_waithcr(SDIOHCR_SOFTWARERESET, 1, 1, 7);
	IOS_Close(__sd0_fd);
	__sd0_fd = IOS_Open(_sd0_fs,1);
	return false;
}
void SDHCShutdown( void )
{
	free(iovec);
	free(request);
	free(response);
}
bool sdio_ReadSectors(sec_t sector, sec_t numSectors, void* buffer )
{
	s32 ret;
	u8 *ptr;
	sec_t blk_off;
 
	if(buffer==NULL)
		return false;
 
	ret = __sd0_select();
	if(ret<0)
		return false;

	if((u32)buffer & 0x1F)
	{
		ptr = (u8*)buffer;

		int secs_to_read;

		while( numSectors > 0 )
		{
			if(__sd0_sdhc == 0)
				blk_off = (sector*PAGE_SIZE512);
			else
				blk_off = sector;

			if(numSectors > 8)
				secs_to_read = 8;
			else
				secs_to_read = numSectors;
			

			sync_before_read( rw_buffer, secs_to_read * 512 );
			_ahbMemFlush(9);

			ret = __sdio_sendcommand( SDIO_CMD_READMULTIBLOCK, SDIOCMD_TYPE_AC, SDIO_RESPONSE_R1, blk_off, secs_to_read, PAGE_SIZE512, rw_buffer, NULL, 0 );
			if( ret >= 0 )
			{
				memcpy( ptr, rw_buffer, PAGE_SIZE512*secs_to_read );
				sync_after_write( ptr, PAGE_SIZE512*secs_to_read );

				ptr			+= PAGE_SIZE512*secs_to_read;
				sector		+= secs_to_read;
				numSectors	-= secs_to_read;

			} else
				break;
		}
	} else {

		if(__sd0_sdhc == 0)
			sector *= PAGE_SIZE512;
		
		sync_before_read( buffer, numSectors * PAGE_SIZE512 );
		_ahbMemFlush(9);

		ret = __sdio_sendcommand( SDIO_CMD_READMULTIBLOCK, SDIOCMD_TYPE_AC, SDIO_RESPONSE_R1, sector, numSectors, PAGE_SIZE512, buffer, NULL, 0 );
	
		sync_after_write( buffer, numSectors * PAGE_SIZE512 );

	}

	__sd0_deselect();
 
	return (ret>=0);
}
 
bool sdio_WriteSectors(sec_t sector, sec_t numSectors,const void* buffer)
{
	s32 ret;
	u8 *ptr;
	u32 blk_off;
 
	if(buffer==NULL)
		return false;
 
	ret = __sd0_select();
	if(ret<0)
		return false;

	if((u32)buffer & 0x1F)
	{
		ptr = (u8*)buffer;
		int secs_to_write;

		while(numSectors>0)
		{
			if(__sd0_sdhc == 0)
				blk_off = (sector*PAGE_SIZE512);
			else
				blk_off = sector;
			if(numSectors > 8)
				secs_to_write = 8;
			else
				secs_to_write = numSectors;

			memcpy( rw_buffer, ptr, PAGE_SIZE512*secs_to_write );

			sync_after_write( rw_buffer, PAGE_SIZE512*secs_to_write );
			_ahbMemFlush(9);

			ret = __sdio_sendcommand(SDIO_CMD_WRITEMULTIBLOCK,SDIOCMD_TYPE_AC,SDIO_RESPONSE_R1,blk_off,secs_to_write,PAGE_SIZE512,rw_buffer,NULL,0);
			if(ret>=0)
			{
				ptr += PAGE_SIZE512*secs_to_write;
				sector+=secs_to_write;
				numSectors-=secs_to_write;
			} else
				break;
		}
	} else {

		if(__sd0_sdhc == 0)
			sector *= PAGE_SIZE512;
		
		sync_after_write( (void*)buffer, PAGE_SIZE512 );
		_ahbMemFlush(9);

		ret = __sdio_sendcommand(SDIO_CMD_WRITEMULTIBLOCK,SDIOCMD_TYPE_AC,SDIO_RESPONSE_R1,sector,numSectors,PAGE_SIZE512,(char *)buffer,NULL,0);

	}

	__sd0_deselect();
 
	return (ret>=0);
}
