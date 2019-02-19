#ifndef _FILEOP_H_
#define _FILEOP_H_

#include <gccore.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

s32 WDVD_Init(void);
s32 WDVD_OpenDataPartition();
s32 WDVD_Close(void);
s32 WDVD_Read(void *buf, u32 len, u64 offset);
s32 WDVD_ClosePartition(void);
s32 WDVD_GetHandle();
bool WDVD_IsPartitionOpen();

bool WDVD_FST_IsMounted();
bool WDVD_FST_Mount();
int WDVD_FST_Open(const char *path);
int WDVD_FST_OpenDisc(u32 discNum);
u32 WDVD_FST_LSeek(u32 pos);
int WDVD_FST_Read(u8 *ptr, s32 len);
int WDVD_FST_Close();
bool WDVD_FST_Unmount();

//aligned for smaller reads under 0x20
extern u8 wdvdTmpBuf[];

#ifdef __cplusplus
}
#endif

#endif
