#ifndef __MEMCARDPRO_H__
#define __MEMCARDPRO_H__

#include <gctypes.h>
#include "global.h"

bool CheckForMemCardPro(s32 chan);
void SetMemCardProGameInfo(s32 chan, NIN_CFG* ncfg);

#endif