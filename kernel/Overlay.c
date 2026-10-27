#include "global.h"
#include "string.h"
#include "ff_utf8.h"
#include "../common/include/Overlay.h"
#include "../overlay/overlay.h"
static const char *const slots[2]={"/ninmap-m15-0.bin","/ninmap-m15-1.bin"};
static int loadedSlot=-1;
static int codeInstalled=0;
/* The loader owns the eventual MEM2 reservation until game handoff. */
static OverlayState pendingState __attribute__((aligned(32)));

/*
 * State and saved-mapping recovery only. The code copy is NOT done here.
 *
 * OverlayInit runs at BootStatus(8), while the PPC loader is still alive - and
 * 0x93180000 sits inside libogc's MEM2 arena (0x90002000-0x933E0000), which the
 * loader's allocator is free to hand out until it exits. Copying the module
 * here would be the same mistake that silently destroyed the flight recorder:
 * memory that looks unused because nothing references it, but is heap. The
 * copy happens in OverlayInstallCode, called from the patcher during game
 * load, when the loader is gone.
 */
void OverlayInit(void) {
    OverlayState *s=&pendingState;
    OverlayFile candidate,best;
    FIL file;UINT n;u32 p,i;int have=0,k;
    best.generation=0;loadedSlot=-1;codeInstalled=0;
    memset32(s,0,sizeof(*s));
    for(p=0;p<4;p++)for(i=0;i<20;i++)s->active[p][i]=i<12?i:0;
    for(k=0;k<2;k++)if(f_open_char(&file,slots[k],FA_READ|FA_OPEN_EXISTING)==FR_OK) {
        int valid=file.obj.objsize==sizeof(candidate) && f_read(&file,&candidate,sizeof(candidate),&n)==FR_OK && n==sizeof(candidate) && OverlayFileValid(&candidate);
        f_close(&file);
        if(valid && (!have || (s32)(candidate.generation-best.generation)>0)) {best=candidate;loadedSlot=k;have=1;}
    }
    if(have){
        for(p=0;p<4;p++)for(i=0;i<20;i++)s->active[p][i]=best.map[p][i];
        s->generation=best.generation;
    }
    memcpy(s->edit,s->active,sizeof(s->active));
    /* M17 diagnostic: no framebuffer drawing until the user opens the menu. */
    s->magic=OVL_MAGIC;s->toast=0;
    sync_after_write(s,sizeof(*s));
}

/*
 * Put the PPC module in memory. Called once, from Patch.c, when the VI retrace
 * handler has been located in the game - which happens during DoPatches, after
 * the loader has handed off and can no longer allocate over this region.
 *
 * The whole reservation is zeroed first: objcopy -O binary drops .bss, so the
 * module's zero-initialised statics only exist because this does it.
 */
void OverlayInstallCode(void) {
    if(codeInstalled)return;
    codeInstalled=1;
    memset32((void*)OVL_CODE_ARM,0,0xC000);
    memcpy((void*)OVL_CODE_ARM,overlay,overlay_size);
    sync_after_write((void*)OVL_CODE_ARM,0xC000);
    memcpy((void*)OVL_STATE_ARM,&pendingState,sizeof(pendingState));
    sync_after_write((void*)OVL_STATE_ARM,sizeof(pendingState));
}

/* Alternate checksummed slots retain the previous mapping if a save is partial.
 * Only called after DI stops, before FAT/USB shutdown. No gameplay FAT writes. */
int OverlaySave(void) {
    OverlayState *s=(OverlayState*)OVL_STATE_ARM;
    OverlayFile out,verify;FIL f;UINT n;u32 p,i,v;int slot;FRESULT wr,sy,cl;
    if(!codeInstalled)return 0;
    sync_before_read(s,sizeof(*s));
    if(s->magic!=OVL_MAGIC || !s->dirty)return 0;
    memset32(&out,0,sizeof(out));
    out.magic=OVL_FILE_MAGIC;out.version=1;out.generation=s->generation+1;
    for(p=0;p<4;p++)for(i=0;i<20;i++) {
        v=s->active[p][i];
        /* Validate before narrowing; never silently truncate a damaged word. */
        if((i<12 && v>12) || (i>=12 && i<17 && v>1) || (i>=17 && v))return -1;
        out.map[p][i]=(u8)v;
    }
    out.checksum=OverlayChecksum(&out);
    if(!OverlayFileValid(&out))return -1;
    slot=loadedSlot==0?1:0;
    if(f_open_char(&f,slots[slot],FA_WRITE|FA_CREATE_ALWAYS)!=FR_OK)return -2;
    wr=f_write(&f,&out,sizeof(out),&n);sy=f_sync(&f);cl=f_close(&f);
    if(wr!=FR_OK || n!=sizeof(out) || sy!=FR_OK || cl!=FR_OK)return -3;
    if(f_open_char(&f,slots[slot],FA_READ|FA_OPEN_EXISTING)!=FR_OK)return -4;
    wr=f_read(&f,&verify,sizeof(verify),&n);cl=f_close(&f);
    if(wr!=FR_OK || n!=sizeof(verify) || cl!=FR_OK || memcmp(&out,&verify,sizeof(out)))return -5;
    loadedSlot=slot;s->generation=out.generation;s->dirty=0;
    sync_after_write(s,sizeof(*s));
    return 1;
}
