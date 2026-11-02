/*
 * Copyright (c) 2022, Extrems <extrems@extremscorner.org>
 *
 * This file is part of Swiss.
 *
 * Swiss is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Swiss is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * with Swiss.  If not, see <https://www.gnu.org/licenses/>.
 */

/*
 * Adapted for Nintendont from Swiss, mcp.c and gameid.c, by way of the
 * USB Loader GX port. Busy retries are bounded here, and only identification
 * and game information commands are sent, never anything that touches saves.
 *
 * Runs in the loader before the kernel is told to start the game. Until then
 * the kernel only waits for the loader and doesn't use the EXI hardware, other
 * than USB Gecko logging on channel 1, which is why Slot B is skipped if there
 * is a USB Gecko.
 */

#include <string.h>
#include <unistd.h>
#include <ogc/card.h>
#include <ogc/exi.h>

#include "../../common/include/CommonConfig.h"
#include "exi.h"
#include "MemCardPro.h"

#define MCP_RESULT_READY	0
#define MCP_RESULT_BUSY		-1
#define MCP_RESULT_NOCARD	-3

// CARD_ProbeEx() reports a card as busy right after it's first detected.
#define MCP_BUSY_RETRIES	16
#define MCP_BUSY_DELAY_US	1000

/*
 * A device leaves the bus while it switches virtual memory cards. FlipperMCE
 * takes at least 500ms for this, and longer if it has to create a new card. It
 * doesn't switch at all if the requested card is already selected, so if it
 * doesn't leave the bus soon, there's nothing to wait for.
 */
#define MCP_SWITCH_DELAY_US	20000
#define MCP_SWITCH_START_POLLS	25	// 500ms
#define MCP_SWITCH_DONE_POLLS	750	// 15s

static const char digits[] = "0123456789ABCDEF";

bool MCP_BuildGameIDCmd(const u8 *hdr, u32 hdrLen, u8 cmd[MCP_GAMEID_CMD_SIZE])
{
	u32 i;

	if (hdrLen < MCP_DISC_HDR_SIZE)
		return false;

	// Check the GameCube magic number. (Triforce AMB1 images are excluded.)
	if (hdr[0x1C] != 0xC2 || hdr[0x1D] != 0x33 ||
	    hdr[0x1E] != 0x9F || hdr[0x1F] != 0x3D)
	{
		return false;
	}

	// The game ID and company code must be alphanumeric.
	for (i = 0; i < 6; i++)
	{
		const u8 c = hdr[i];
		if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
		      (c >= 'a' && c <= 'z')))
		{
			return false;
		}
	}

	cmd[0] = 0x8B;
	cmd[1] = 0x11;
	memcpy(&cmd[2], hdr, 6);
	cmd[8]  = digits[hdr[0x06] / 16];
	cmd[9]  = digits[hdr[0x06] % 16];
	cmd[10] = digits[hdr[0x07] / 16];
	cmd[11] = digits[hdr[0x07] % 16];
	return true;
}

bool MCP_BuildTitle(const u8 *hdr, u32 hdrLen, char title[MCP_TITLE_SIZE])
{
	u32 i;

	memset(title, 0, MCP_TITLE_SIZE);
	if (hdrLen < MCP_DISC_HDR_SIZE)
		return false;

	// The title in the header isn't necessarily NULL-terminated.
	for (i = 0; i < MCP_TITLE_SIZE - 1 && hdr[0x20 + i] != 0; i++)
		title[i] = (char)hdr[0x20 + i];

	return i > 0;
}

u32 MCP_GetEligibleSlots(u32 config, bool isWiiU, bool isTriforce, bool hasGecko)
{
	u32 slots;

	if (isWiiU || isTriforce)
		return 0;

	if (!(config & NIN_CFG_MEMCARDEMU))
		slots = MCP_SLOT_A | MCP_SLOT_B;
	else if (NIN_CFG_HYBRID_EXI_ACTIVE(config, false, false))
		slots = MCP_SLOT_B;	// Slot A is virtual.
	else
		return 0;

	if (hasGecko || (config & NIN_CFG_DEBUGGER))
		slots &= ~MCP_SLOT_B;

	return slots;
}

/**
 * Run one EXI transaction. The bus is always released before returning.
 * @param chan		[in] EXI channel.
 * @param speed		[in] EXI speed.
 * @param cmd		[in] Command to send.
 * @param cmdLen	[in] Command length.
 * @param data		[in,out,opt] Data to transfer after the command.
 * @param dataLen	[in] Data length.
 * @param dataMode	[in] EXI_READ or EXI_WRITE.
 * @return MCP_RESULT_*
 */
static s32 MCP_Transfer(s32 chan, u32 speed, const u8 *cmd, u32 cmdLen,
			void *data, u32 dataLen, u32 dataMode)
{
	bool err;

	if (!EXI_Lock(chan, EXI_DEVICE_0, NULL))
		return MCP_RESULT_BUSY;
	if (!EXI_Select(chan, EXI_DEVICE_0, speed))
	{
		EXI_Unlock(chan);
		return MCP_RESULT_NOCARD;
	}

	err = !EXI_ImmEx(chan, (void*)cmd, cmdLen, EXI_WRITE);
	if (!err && data)
		err = !EXI_ImmEx(chan, data, dataLen, dataMode);
	err |= !EXI_Deselect(chan);
	EXI_Unlock(chan);

	return err ? MCP_RESULT_NOCARD : MCP_RESULT_READY;
}

/**
 * Run one EXI transaction, retrying a bounded number of times if busy.
 * Parameters are the same as MCP_Transfer().
 * @return MCP_RESULT_*
 */
static s32 MCP_TransferRetry(s32 chan, u32 speed, const u8 *cmd, u32 cmdLen,
			     void *data, u32 dataLen, u32 dataMode)
{
	s32 ret = MCP_RESULT_BUSY;
	u32 i;

	for (i = 0; i < MCP_BUSY_RETRIES && ret == MCP_RESULT_BUSY; i++)
	{
		if (i > 0)
			usleep(MCP_BUSY_DELAY_US);
		ret = MCP_Transfer(chan, speed, cmd, cmdLen, data, dataLen, dataMode);
	}
	return ret;
}

/**
 * Check for a compatible device.
 * @param chan	[in] EXI channel.
 * @return True if the device identified itself; false if not.
 */
static bool MCP_CheckDeviceID(s32 chan)
{
	static const u8 cmd[2] = {0x8B, 0x00};
	u8 id[4] = {0, 0, 0, 0};

	if (MCP_Transfer(chan, EXI_SPEED1MHZ, cmd, sizeof(cmd),
			 id, sizeof(id), EXI_READ) != MCP_RESULT_READY)
	{
		return false;
	}
	return id[0] == 0x38 && id[1] == 0x42;
}

/**
 * Check for a memory card that responds to the device ID command.
 * Devices that aren't memory cards are never sent the command.
 * @param chan	[in] EXI channel.
 * @return True if a compatible device is present; false if not.
 */
static bool MCP_Detect(s32 chan)
{
	s32 ret = CARD_ERROR_BUSY;
	u32 i;

	for (i = 0; i < MCP_BUSY_RETRIES && ret == CARD_ERROR_BUSY; i++)
	{
		if (i > 0)
			usleep(MCP_BUSY_DELAY_US);
		ret = CARD_ProbeEx(chan, NULL, NULL);
	}
	if (ret < 0)
		return false;

	for (i = 0; i < MCP_BUSY_RETRIES; i++)
	{
		if (i > 0)
			usleep(MCP_BUSY_DELAY_US);
		if (MCP_CheckDeviceID(chan))
			return true;
	}
	return false;
}

/**
 * Wait for a device to finish switching virtual memory cards.
 * @param chan	[in] EXI channel.
 */
static void MCP_WaitForSwitch(s32 chan)
{
	u32 i;

	for (i = 0; i < MCP_SWITCH_START_POLLS; i++)
	{
		usleep(MCP_SWITCH_DELAY_US);
		if (!MCP_CheckDeviceID(chan))
			break;
	}
	if (i == MCP_SWITCH_START_POLLS)
		return;

	for (i = 0; i < MCP_SWITCH_DONE_POLLS; i++)
	{
		usleep(MCP_SWITCH_DELAY_US);
		if (MCP_CheckDeviceID(chan))
			return;
	}
	gprintf("MemCardPro: Timed out waiting for slot %d to switch cards.\r\n", chan);
}

void MCP_SendGameInfo(const u8 *hdr, u32 hdrLen, u32 slots)
{
	static const u8 titleCmd[2] = {0x8B, 0x13};
	u8 idCmd[MCP_GAMEID_CMD_SIZE];
	char title[MCP_TITLE_SIZE];
	bool hasTitle;
	s32 chan;

	if (slots == 0)
		return;
	if (!MCP_BuildGameIDCmd(hdr, hdrLen, idCmd))
	{
		gprintf("MemCardPro: Disc header is invalid, not sending game ID.\r\n");
		return;
	}
	hasTitle = MCP_BuildTitle(hdr, hdrLen, title);

	for (chan = EXI_CHANNEL_0; chan <= EXI_CHANNEL_1; chan++)
	{
		if (!(slots & (1 << chan)) || !MCP_Detect(chan))
			continue;

		if (MCP_TransferRetry(chan, EXI_SPEED16MHZ, idCmd, sizeof(idCmd),
				      NULL, 0, EXI_WRITE) != MCP_RESULT_READY)
		{
			gprintf("MemCardPro: Failed to send game ID to slot %d.\r\n", chan);
			continue;
		}
		gprintf("MemCardPro: Sent game ID %.10s to slot %d.\r\n", &idCmd[2], chan);

		// Setting the ID clears the title, so this has to be sent second.
		// The title is cosmetic, so a failure here doesn't matter.
		if (hasTitle)
		{
			MCP_TransferRetry(chan, EXI_SPEED16MHZ, titleCmd, sizeof(titleCmd),
					  title, sizeof(title), EXI_WRITE);
		}

		MCP_WaitForSwitch(chan);
	}
}
