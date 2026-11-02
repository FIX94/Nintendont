// Host-side stand-in for libogc's card.h. Values match libogc 1.8.23.
#ifndef __CARD_H__
#define __CARD_H__

#include <gctypes.h>

#define CARD_ERROR_READY	0
#define CARD_ERROR_BUSY		-1
#define CARD_ERROR_WRONGDEVICE	-2
#define CARD_ERROR_NOCARD	-3

s32 CARD_ProbeEx(s32 chn, s32 *mem_size, s32 *sect_size);

#endif
