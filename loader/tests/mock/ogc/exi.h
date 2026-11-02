// Host-side stand-in for libogc's exi.h. Values match libogc 1.8.23.
#ifndef __EXI_H__
#define __EXI_H__

#include <gctypes.h>

#define EXI_READ	0
#define EXI_WRITE	1

#define EXI_CHANNEL_0	0
#define EXI_CHANNEL_1	1

#define EXI_DEVICE_0	0

#define EXI_SPEED1MHZ	0
#define EXI_SPEED16MHZ	4

typedef s32 (*EXICallback)(s32 chn, s32 dev);

s32 EXI_Lock(s32 nChn, s32 nDev, EXICallback unlockCB);
s32 EXI_Unlock(s32 nChn);
s32 EXI_Select(s32 nChn, s32 nDev, s32 nFrq);
s32 EXI_Deselect(s32 nChn);
s32 EXI_ImmEx(s32 nChn, void *pData, u32 nLen, u32 nMode);

#endif
