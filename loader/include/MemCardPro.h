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
 * Adapted for Nintendont from Swiss, mcp.h and gameid.c, by way of the
 * USB Loader GX port.
 */

#ifndef __MEMCARDPRO_H__
#define __MEMCARDPRO_H__

#include <gctypes.h>

// Bytes of the disc header needed: boot.bin through the end of the title.
#define MCP_DISC_HDR_SIZE	0x60

#define MCP_GAMEID_CMD_SIZE	12
#define MCP_TITLE_SIZE		64

// Bitmask of physical memory card slots.
#define MCP_SLOT_A		(1 << 0)
#define MCP_SLOT_B		(1 << 1)

/**
 * Build the "set game ID" command from a disc header.
 * @param hdr		[in] Disc header.
 * @param hdrLen	[in] Valid bytes in hdr.
 * @param cmd		[out] Command. Fully initialized on success.
 * @return True on success; false if hdr isn't a valid GameCube disc header.
 */
bool MCP_BuildGameIDCmd(const u8 *hdr, u32 hdrLen, u8 cmd[MCP_GAMEID_CMD_SIZE]);

/**
 * Get the game title from a disc header.
 * The title is copied as-is, since it may be in Shift-JIS.
 * @param hdr		[in] Disc header.
 * @param hdrLen	[in] Valid bytes in hdr.
 * @param title		[out] NULL-terminated title, zero-padded to MCP_TITLE_SIZE.
 * @return True on success; false if the header has no title.
 */
bool MCP_BuildTitle(const u8 *hdr, u32 hdrLen, char title[MCP_TITLE_SIZE]);

/**
 * Get the slots that the game will access as physical memory cards.
 * @param config	[in] NIN_CFG.Config.
 * @param isWiiU	[in] True on Wii U/vWii/Wii VC, which have no card slots.
 * @param isTriforce	[in] True for Triforce games.
 * @param hasGecko	[in] True if a USB Gecko is in Slot B.
 * @return Bitmask of MCP_SLOT_* values.
 */
u32 MCP_GetEligibleSlots(u32 config, bool isWiiU, bool isTriforce, bool hasGecko);

/**
 * Send the game ID and title to any compatible device in the specified slots,
 * then wait for it to finish switching to the game's virtual memory card.
 * Failures are not fatal, and an invalid header sends nothing.
 * @param hdr		[in] Disc header.
 * @param hdrLen	[in] Valid bytes in hdr.
 * @param slots		[in] Bitmask of MCP_SLOT_* values.
 */
void MCP_SendGameInfo(const u8 *hdr, u32 hdrLen, u32 slots);

#endif
