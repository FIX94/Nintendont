/*
BT.c for Nintendont (Kernel)

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

/* Wiimote and Extension Documentation from wiibrew.org */
/* WiiU Pro Controller Documentation from TeHaxor69 */
/* lwBT ported from LibOGC */

#include "global.h"
#include "string.h"
#include "BT.h"
#include "lwbt/btarch.h"
#include "lwbt/hci.h"
#include "lwbt/l2cap.h"
#include "lwbt/physbusif.h"

extern int dbgprintf( const char *fmt, ...);

static vu32 BTChannelsUsed = 0;
extern vu32 intr, bulk;

static conf_pads *BTDevices = (conf_pads*)0x132C0000;
struct BTPadStat *BTPadConnected[4];

struct BTPadStat BTPadStatus[CONF_PAD_MAX_REGISTERED] ALIGNED(32);
struct linkkey_info BTKeys[CONF_PAD_MAX_REGISTERED] ALIGNED(32);

struct BTPadCont *BTPad = (struct BTPadCont*)0x132F0000;

static vu32* BTMotor = (u32*)0x13002720;
static vu32* BTPadFree = (u32*)0x13002730;

u8 LEDState[] = { 0x10, 0x20, 0x40, 0x80, 0xF0 };

#define CHAN_NOT_SET 4

#define TRANSFER_CONNECT 0
#define TRANSFER_EXT1 1
#define TRANSFER_EXT2 2
#define TRANSFER_SET_IDENT 3
#define TRANSFER_GET_IDENT 4
#define TRANSFER_CALIBRATE 5
#define TRANSFER_DONE 6

#define C_NOT_SET	(0<<0)
#define C_CCP		(1<<0)
#define C_CC		(1<<1)
#define C_SWAP		(1<<2)
#define C_RUMBLE_WM	(1<<3)

const s8 DEADZONE = 0x1A;

static void BTSetControllerState(struct bte_pcb *sock, u32 State)
{
	u8 buf[2];
	buf[0] = 0x11;
	buf[1] = State;
	bte_senddata(sock,buf,2);
}
static s32 BTHandleData(void *arg,void *buffer,u16 len)
{
	sync_before_read(arg, sizeof(struct BTPadStat));
	struct BTPadStat *stat = (struct BTPadStat*)arg;
	u32 chan = stat->channel;

	if(*(u8*)buffer == 0x3D)	//21 expansion bytes report
	{
		if(stat->transferstate == TRANSFER_CALIBRATE)
		{
			stat->xAxisLmid = swab16(R16((u32)(((u8*)buffer)+1)));
			stat->xAxisRmid = swab16(R16((u32)(((u8*)buffer)+3)));
			stat->yAxisLmid = swab16(R16((u32)(((u8*)buffer)+5)));
			stat->yAxisRmid = swab16(R16((u32)(((u8*)buffer)+7)));
			stat->transferstate = TRANSFER_DONE;
			sync_after_write(arg, sizeof(struct BTPadStat));
			sync_before_read(arg, sizeof(struct BTPadStat));
		}
		if(chan == CHAN_NOT_SET)
			return ERR_OK;
		sync_before_read(&BTPad[chan], sizeof(struct BTPadCont));
		BTPad[chan].xAxisL = ((swab16(R16((u32)(((u8*)buffer)+1))) - stat->xAxisLmid) *7) >>6;
		BTPad[chan].xAxisR = ((swab16(R16((u32)(((u8*)buffer)+3))) - stat->xAxisRmid) *7) >>6;
		BTPad[chan].yAxisL = ((swab16(R16((u32)(((u8*)buffer)+5))) - stat->yAxisLmid) *7) >>6;
		BTPad[chan].yAxisR = ((swab16(R16((u32)(((u8*)buffer)+7))) - stat->yAxisRmid) *7) >>6;
		u32 prevButton = BTPad[chan].button;
		BTPad[chan].button = ~(R16((u32)(((u8*)buffer)+9)));
		if((!(prevButton & BT_BUTTON_SELECT)) && BTPad[chan].button & BT_BUTTON_SELECT)
		{
			dbgprintf("Using %s control scheme\n", (stat->controller & C_SWAP) ? "orginal" : "swapped");
			stat->controller = (stat->controller & C_SWAP) ? (stat->controller & ~C_SWAP) : (stat->controller | C_SWAP);
			sync_after_write(arg, sizeof(struct BTPadStat));
			sync_before_read(arg, sizeof(struct BTPadStat));
		}
		BTPad[chan].used = stat->controller;
		sync_after_write(&BTPad[chan], sizeof(struct BTPadCont));
	}
	else if(*(u8*)buffer == 0x34)	//core buttons with 19 exptension bytes report
	{
		if(stat->transferstate == TRANSFER_CALIBRATE)
		{
			stat->xAxisLmid = *((u8*)buffer+3)&0x3F;
			stat->xAxisRmid = ((*((u8*)buffer+5)&0x80)>>7) | ((*((u8*)buffer+4)&0xC0)>>5) | ((*((u8*)buffer+3)&0xC0)>>3);
			stat->yAxisLmid = *((u8*)buffer+4)&0x3F;
			stat->yAxisRmid = *((u8*)buffer+5)&0x1F;
			stat->transferstate = TRANSFER_DONE;
			sync_after_write(arg, sizeof(struct BTPadStat));
			sync_before_read(arg, sizeof(struct BTPadStat));
		}
		if(chan == CHAN_NOT_SET || stat->controller == C_NOT_SET)
			return ERR_OK;
		sync_before_read(&BTPad[chan], sizeof(struct BTPadCont));
		BTPad[chan].xAxisL = ((*((u8*)buffer+3)&0x3F) - stat->xAxisLmid) <<2;
		BTPad[chan].xAxisR = ((((*((u8*)buffer+5)&0x80)>>7) | ((*((u8*)buffer+4)&0xC0)>>5) | ((*((u8*)buffer+3)&0xC0)>>3)) - stat->xAxisRmid) <<3;
		BTPad[chan].yAxisL = ((*((u8*)buffer+4)&0x3F) - stat->yAxisLmid) <<2;
		BTPad[chan].yAxisR = ((*((u8*)buffer+5)&0x1F) - stat->yAxisRmid) <<3;
		if(stat->controller & C_CC)
		{
			/* Calculate left trigger with deadzone */
			u8 tmp_triggerL = (((*((u8*)buffer+6)&0xE0)>>5) | ((*((u8*)buffer+5)&0x60)>>2))<<3;
			if(tmp_triggerL > DEADZONE)
				BTPad[chan].triggerL = (tmp_triggerL - DEADZONE) * 1.11f;
			else
				BTPad[chan].triggerL = 0;
			/* Calculate right trigger with deadzone */
			u8 tmp_triggerR = (*((u8*)buffer+6)&0x1F)<<3;
			if(tmp_triggerR > DEADZONE)
				BTPad[chan].triggerR = (tmp_triggerR - DEADZONE) * 1.11f;
			else
				BTPad[chan].triggerR = 0;
		}
		u32 prevButton = BTPad[chan].button;
		BTPad[chan].button = ~(R16((u32)(((u8*)buffer)+7))) | (*((u8*)buffer+2) & 0x10)<<4; //unused 0x100 for wiimote minus
		if((!(prevButton & BT_BUTTON_SELECT)) && BTPad[chan].button & BT_BUTTON_SELECT)
		{
			dbgprintf("Using %s control scheme\n", (stat->controller & C_SWAP) ? "orginal" : "swapped");
			stat->controller = (stat->controller & C_SWAP) ? (stat->controller & ~C_SWAP) : (stat->controller | C_SWAP);
			sync_after_write(arg, sizeof(struct BTPadStat));
			sync_before_read(arg, sizeof(struct BTPadStat));
		}
		if((!(prevButton & WM_BUTTON_MINUS)) && BTPad[chan].button & WM_BUTTON_MINUS)
		{
			dbgprintf("%s rumble for wiimote\n", (stat->controller & C_RUMBLE_WM) ? "Disabling" : "Enabling");
			stat->controller = (stat->controller & C_RUMBLE_WM) ? (stat->controller & ~C_RUMBLE_WM) : (stat->controller | C_RUMBLE_WM);
			sync_after_write(arg, sizeof(struct BTPadStat));
			sync_before_read(arg, sizeof(struct BTPadStat));
		}
		BTPad[chan].used = stat->controller;
		sync_after_write(&BTPad[chan], sizeof(struct BTPadCont));
	}
	else if(*(u8*)buffer == 0x30)	//core buttons report
	{
		if(stat->transferstate == TRANSFER_CONNECT)
		{
			u8 buf[2];
			buf[0] = 0x15;
			buf[1] = 0x00;
			bte_senddata(stat->sock,buf,2);
			stat->transferstate = TRANSFER_EXT1;
			sync_after_write(arg, sizeof(struct BTPadStat));
		}
	}
	else if(*(u8*)buffer == 0x20)	//status report
	{
		if(*((u8*)buffer+3) & 0x02)	//expansion controller connected
		{
			if(stat->transferstate == TRANSFER_EXT1)
			{
				u8 data[22];
				memset(data, 0, 22);
				data[0] = 0x16; //set mode to write
				data[1] = 0x04; //write to registers
				data[2] = 0xA4; data[3] = 0x00; data[4] = 0xF0; //address
				data[5] = 0x01; //length
				data[6] = 0x55; //data
				bte_senddata(stat->sock,data,22);
				stat->transferstate = TRANSFER_EXT2;
				sync_after_write(arg, sizeof(struct BTPadStat));
			}
		}
		else if(stat->transfertype == 0x34)
		{
			//reset
			stat->controller = C_NOT_SET;
			stat->transferstate = TRANSFER_EXT1;
			sync_after_write(arg, sizeof(struct BTPadStat));
			if(chan < CHAN_NOT_SET)
			{
				BTPad[chan].used = C_NOT_SET;
				sync_after_write(&BTPad[chan], sizeof(struct BTPadCont));
			}
		}
	}
	else if(*(u8*)buffer == 0x21)	//read memory data
	{
		if(stat->transferstate == TRANSFER_GET_IDENT)
		{
			if(R32((u32)((u8*)buffer+8)) == 0xA4200101)
			{
				if(*((u8*)buffer+6) == 0)
				{
					stat->controller = C_CC;
					dbgprintf("Connected Classic Controller\n");
				}
				else
				{
					stat->controller = C_CCP;
					dbgprintf("Connected Classic Controller Pro\n");
				}
				/* Finally enable reading */
				u8 buf[3];
				buf[0] = 0x12;
				buf[1] = 0x00;
				buf[2] = stat->transfertype;
				bte_senddata(stat->sock,buf,3);
			}
			stat->transferstate = TRANSFER_CALIBRATE;
			sync_after_write(arg, sizeof(struct BTPadStat));
		}
	}
	else if(*(u8*)buffer == 0x22)	//acknowledge output report, return function result 
	{
		if(*((u8*)buffer+3) & 0x02)
		{
			if(stat->transferstate == TRANSFER_EXT2)
			{
				u8 data[22];
				memset(data, 0, 22);
				data[0] = 0x16; //set mode to write
				data[1] = 0x04; //write to registers
				data[2] = 0xA4; data[3] = 0x00; data[4] = 0xFB; //address
				data[5] = 0x01; //length
				data[6] = 0x00; //data
				bte_senddata(stat->sock,data,22);
				stat->transferstate = TRANSFER_SET_IDENT;
				sync_after_write(arg, sizeof(struct BTPadStat));
			}
			else if(stat->transferstate == TRANSFER_SET_IDENT)
			{
				u8 data[7];
				data[0] = 0x17; //set mode to read
				data[1] = 0x04; //read from registers
				data[2] = 0xA4; data[3] = 0x00; data[4] = 0xFA; //address
				data[5] = 0x00; data[6] = 0x06; //length
				bte_senddata(stat->sock,data,7);
				stat->transferstate = TRANSFER_GET_IDENT;
				sync_after_write(arg, sizeof(struct BTPadStat));
			}
		}
		else if(stat->transfertype == 0x34)
		{
			//reset
			stat->controller = C_NOT_SET;
			stat->transferstate = TRANSFER_EXT1;
			sync_after_write(arg, sizeof(struct BTPadStat));
			if(chan < CHAN_NOT_SET)
			{
				BTPad[chan].used = C_NOT_SET;
				sync_after_write(&BTPad[chan], sizeof(struct BTPadCont));
			}
		}
	}
	return ERR_OK;
}

static s32 BTHandleConnect(void *arg,struct bte_pcb *pcb,u8 err)
{
	sync_before_read(arg, sizeof(struct BTPadStat));
	struct BTPadStat *stat = (struct BTPadStat*)arg;

	if(BTChannelsUsed >= 4)
	{
		bte_disconnect(stat->sock);
		return ERR_OK;
	}

	u8 buf[3];

	stat->channel = CHAN_NOT_SET;
	stat->rumble = 0;

	BTSetControllerState(stat->sock, LEDState[CHAN_NOT_SET]);

	//wiimote extensions need some extra stuff first, start with getting its status
	if(stat->transfertype == 0x34)
	{
		buf[0] = 0x12;
		buf[1] = 0x00;
		buf[2] = 0x30; //get normal buttons once
		bte_senddata(stat->sock,buf,2);
		stat->transferstate = TRANSFER_CONNECT;
		stat->controller = C_NOT_SET;
	}
	else
	{
		dbgprintf("Connected WiiU Pro Controller\n");
		buf[0] = 0x12;
		buf[1] = 0x00;
		buf[2] = stat->transfertype;
		bte_senddata(stat->sock,buf,3);
		stat->transferstate = TRANSFER_CALIBRATE;
		stat->controller = C_CCP;
	}

	BTPadConnected[BTChannelsUsed] = stat;
	sync_after_write(stat, sizeof(struct BTPadStat));
	BTChannelsUsed++;
	return ERR_OK;
}

static s32 BTHandleDisconnect(void *arg,struct bte_pcb *pcb,u8 err)
{
	dbgprintf("Controller disconnected\n");
	if(BTChannelsUsed) BTChannelsUsed--;
	u32 i;
	for(i = 0; i < 4; ++i)
	{
		if(BTPadConnected[i] == arg)
		{
			u32 chan = BTPadConnected[i]->channel;
			if(chan != CHAN_NOT_SET)
			{
				BTPad[chan].used = C_NOT_SET;
				sync_after_write(&BTPad[chan], 0x20);
			}
			while(i+1 < 4)
			{
				BTPadConnected[i] = BTPadConnected[i+1];
				BTPadConnected[i+1] = NULL;
				i++;
			}
			break;
		}
	}
	return ERR_OK;
}

int RegisterBTPad(struct BTPadStat *stat, struct bd_addr *_bdaddr)
{
	stat->bdaddr = *_bdaddr;
	stat->sock = bte_new();

	if(stat->sock == NULL)
		return ERR_OK;

	bte_arg(stat->sock, stat);
	bte_received(stat->sock, BTHandleData);
	bte_disconnected(stat->sock, BTHandleDisconnect);

	bte_registerdeviceasync(stat->sock, _bdaddr, BTHandleConnect);
	sync_after_write(stat, sizeof(struct BTPadStat));

	return ERR_OK;
}

static s32 BTCompleteCB(s32 result,void *usrdata)
{
	u32 i;
	struct bd_addr bdaddr;

	if(result == ERR_OK)
	{
		for(i = 0; i <BTDevices->num_registered; i++)
		{
			BD_ADDR(&(bdaddr),BTDevices->registered[i].bdaddr[5],BTDevices->registered[i].bdaddr[4],BTDevices->registered[i].bdaddr[3],
							BTDevices->registered[i].bdaddr[2],BTDevices->registered[i].bdaddr[1],BTDevices->registered[i].bdaddr[0]);

			if(strstr(BTDevices->registered[i].name, "-UC") != NULL)
				BTPadStatus[i].transfertype = 0x3D;
			else
				BTPadStatus[i].transfertype = 0x34;
			BTPadStatus[i].channel = CHAN_NOT_SET;
			RegisterBTPad(&BTPadStatus[i],&(bdaddr));
		}
	}
	return ERR_OK;
}

static s32 BTPatchCB(s32 result,void *usrdata)
{
	BTE_InitSub(BTCompleteCB);
	return ERR_OK;
}

static s32 BTReadLinkKeyCB(s32 result,void *usrdata)
{
	BTE_ApplyPatch(BTPatchCB);
	return ERR_OK;
}

static s32 BTInitCoreCB(s32 result, void *usrdata)
{
	if(result == ERR_OK)
		BTE_ReadStoredLinkKey(BTKeys, CONF_PAD_MAX_REGISTERED, BTReadLinkKeyCB);
	return ERR_OK;
}

u32 BTTimer = 0;
u32 inited = 0;
void BTInit(void)
{
	memset(BTKeys, 0, sizeof(struct linkkey_info) * CONF_PAD_MAX_REGISTERED);

	memset(BTPad, 0, sizeof(struct BTPadCont)*4);
	sync_after_write(BTPad, sizeof(struct BTPadCont)*4);

	/* Both Motor and Channel free */
	memset((void*)BTMotor, 0, 0x20);
	sync_after_write((void*)BTMotor, 0x20);

	BTE_Init();
	BTE_InitCore(BTInitCoreCB);

	BTTimer = read32(HW_TIMER);
	u32 CheckTimer = read32(HW_TIMER);
	inited = 1;
	while(1)
	{
		if((read32(HW_TIMER) - CheckTimer) / 1898437)
			break;
		BTUpdateRegisters();
		udelay(10);
	}
}

void BTUpdateRegisters(void)
{
	if(inited == 0)
		return;

	if(intr == 1)
	{
		intr = 0;
		__readintrdataCB();
		__issue_intrread();
	}
	if(bulk == 1)
	{
		bulk = 0;
		__readbulkdataCB();
		__issue_bulkread();
	}

	u32 i = 0, j = 0;
	sync_before_read((void*)0x13002700,0x40);
	for( ; i < BTChannelsUsed; ++i)
	{
		sync_before_read(BTPadConnected[i], sizeof(struct BTPadStat));
		u32 LastChan = BTPadConnected[i]->channel;
		u32 LastRumble = BTPadConnected[i]->rumble;
		u32 CurChan = CHAN_NOT_SET;
		u32 CurRumble = 0;
		if(BTPadConnected[i]->controller != C_NOT_SET)
		{
			for( ; j < 4; ++j)
			{
				if(BTPadFree[j] == 1)
				{
					CurChan = j;
					CurRumble = BTMotor[j];
					j++;
					break;
				}
			}
		}
		if(CurRumble == 0)
		{
			if(LastRumble == 1)
			{
				if((read32(HW_TIMER) - BTPadConnected[i]->rumbletime) > 94922)
					BTPadConnected[i]->rumbletime = 0;
				else //extend to at least 1/20th of a second
					CurRumble = 1;
			}
		}
		else if(CurRumble == 1)
		{
			if(LastRumble != 1)
				BTPadConnected[i]->rumbletime = read32(HW_TIMER);
		}
		else if(CurRumble == 2) //direct stop
			CurRumble = 0;

		if(LastChan != CurChan || LastRumble != CurRumble)
		{
			if(CurChan == CHAN_NOT_SET || ((LastChan != CHAN_NOT_SET) && CurChan < LastChan))
			{
				BTPad[LastChan].used = C_NOT_SET;
				sync_after_write(&BTPad[LastChan], sizeof(struct BTPadCont));
			}
			BTPadConnected[i]->channel = CurChan;
			BTPadConnected[i]->rumble = CurRumble;
			if(BTPadConnected[i]->transfertype == 0x3D || BTPadConnected[i]->controller & C_RUMBLE_WM)
				BTSetControllerState(BTPadConnected[i]->sock, LEDState[CurChan] | CurRumble);
			else //classic controller doesnt have rumble, can be forced to wiimote if wanted
				BTSetControllerState(BTPadConnected[i]->sock, LEDState[CurChan]);
			sync_after_write(BTPadConnected[i], sizeof(struct BTPadStat));
		}
	}
	if((read32(HW_TIMER) - BTTimer) / 1898437)
	{
		//dbgprintf("tick\n");
		l2cap_tmr(); //every second
		BTTimer = read32(HW_TIMER);
	}
}
