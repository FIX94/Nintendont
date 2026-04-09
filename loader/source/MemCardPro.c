/*
 Lightly modified from the implementation in swiss
 https://github.com/emukidid/swiss-gc/commit/3ee5c037e236060e38521f8f29187c1581df414a
*/

#include "MemCardPro.h"

#include <ogc/card.h>
#include <ogc/exi.h>

static const char digits[16] = "0123456789ABCDEF";

bool CheckForMemCardPro(s32 chan) 
{
	if (CARD_ProbeEx(chan, NULL, NULL) < 0) {
		return false;
	}

	bool err = false;
	u32 id;
	u8 cmd[2];

	if (!EXI_Lock(chan, EXI_DEVICE_0, NULL)) return false;
	if (!EXI_Select(chan, EXI_DEVICE_0, EXI_SPEED16MHZ)) {
		EXI_Unlock(chan);
		return false;
	}

	cmd[0] = 0x8B;
	cmd[1] = 0x00;

	err |= !EXI_Imm(chan, cmd, 2, EXI_WRITE, NULL);
	err |= !EXI_Sync(chan);
	err |= !EXI_Imm(chan, &id, 4, EXI_READ, NULL);
	err |= !EXI_Sync(chan);
	err |= !EXI_Deselect(chan);
	EXI_Unlock(chan);

	if (err)
		return false;
	else if (id >> 16 != 0x3842)
		return false;
	else
		return true;
}

void SetMemCardProGameInfo(s32 chan, NIN_CFG* ncfg)
{
	u8 cmd[12];

	if (!EXI_Lock(chan, EXI_DEVICE_0, NULL)) return;
	if (!EXI_Select(chan, EXI_DEVICE_0, EXI_SPEED16MHZ)) {
		EXI_Unlock(chan);
		return;
	}

	memset(cmd, 0, sizeof(cmd));
	cmd[0] = 0x8B;
	cmd[1] = 0x11;

	memcpy(&cmd[2], &ncfg->GameID, 4);
	memcpy(&cmd[6], &ncfg->GameMakerCode,  2);
	cmd[8]  = digits[ncfg->GameDiscNumber / 16];
	cmd[9]  = digits[ncfg->GameDiscNumber % 16];
	cmd[10] = digits[ncfg->GameRevision / 16];
	cmd[11] = digits[ncfg->GameRevision % 16];

	EXI_ImmEx(chan, cmd, sizeof(cmd), EXI_WRITE);
	EXI_Deselect(chan);
	EXI_Unlock(chan);
}