#ifndef NIN_OVERLAY_H
#define NIN_OVERLAY_H
/* Dedicated M15 reservation. Never place the menu in FST/PSO scratch memory. */
#define OVL_CODE_ARM 0x13180000u
#define OVL_CODE_PPC 0x93180000u
#define OVL_STATE_ARM 0x1318F000u
#define OVL_STATE_PPC 0xD318F000u
#define OVL_MAGIC 0x4F563231u /* "OV21": M21 appended the capture word */
#define OVL_FILE_MAGIC 0x4D31354Du
#define OVL_MAP_BYTES 20
#define OVL_BUTTONS 12
#define OVL_ROWS 18
typedef struct {
    unsigned short button;
    signed char stickX, stickY, substickX, substickY;
    unsigned char triggerLeft, triggerRight, analogA, analogB;
    signed char err;
    unsigned char padding;
} OverlayPad;
typedef struct {
    unsigned int magic, enabled, open, owner, port, row, release, dirty;
    unsigned int generation, toast, inputCalls, drawCalls, applies, cancels;
    /* Broadway uncached MEM2 stores MUST be aligned 32-bit accesses.
     * Byte/halfword stores corrupt neighboring data on real Wii hardware.
     * Keep the packed on-disk format separate from this shared runtime ABI. */
    unsigned int previous[4];
    unsigned int active[4][OVL_MAP_BYTES];
    unsigned int edit[4][OVL_MAP_BYTES];
    /* M19 telemetry is captured BEFORE neutralisation/remapping. Word stores only.
     * exitRequested is consumed by PADReadGC's existing clean exit path. */
    unsigned int exitRequested, mode, waitNeutral, action, connected;
    unsigned int live[4][7]; /* buttons, signed main X/Y, C X/Y, analog L/R */
    unsigned int lastDetected;
    /* M20 list menu: cursor within a screen, retrace timestamp for the
     * listen timeout and hold-B, reset/hold in progress, status message. */
    unsigned int cursor, timer, confirm, notice;
    /* M21 press-to-assign: the single button currently held on the edited
     * port, or 0 when nothing is held or two were pressed together. The
     * assignment happens on release, so the closing chord never assigns. */
    unsigned int candidate;
} OverlayState;
_Static_assert(sizeof(OverlayState) == 868, "OverlayState word-only ABI");
typedef struct {
    unsigned int magic, version, generation, checksum;
    unsigned char map[4][OVL_MAP_BYTES];
} OverlayFile;
static inline unsigned int OverlayChecksum(const OverlayFile *f) {
    const unsigned char *p=(const unsigned char*)f;
    unsigned int h=2166136261u, i;
    for(i=0;i<sizeof(*f);i++) if(i<12 || i>=16) h=(h^p[i])*16777619u;
    return h;
}
static inline int OverlayFileValid(const OverlayFile *f) {
    unsigned int p,i;
    if(f->magic!=OVL_FILE_MAGIC || f->version!=1 || f->checksum!=OverlayChecksum(f)) return 0;
    for(p=0;p<4;p++) for(i=0;i<OVL_MAP_BYTES;i++) {
        if(i<12 && f->map[p][i]>12) return 0;
        if(i>=12 && i<17 && f->map[p][i]>1) return 0;
        if(i>=17 && f->map[p][i]!=0) return 0;
    }
    return 1;
}
void OverlayInit(void);
/* Copy the PPC module into its reservation. Safe only once the loader has
 * exited - call from the patcher, never from OverlayInit. Idempotent. */
void OverlayInstallCode(void);
int OverlaySave(void);
#endif
