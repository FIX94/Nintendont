/* SlippiDebug.c
 * Miscellaneous functions to help debug things in PPC-world.
 */

#include "SlippiDebug.h"
#include "global.h"
#include "syscalls.h"
#include "net.h"

/* vsCheckCSS()
 * Quick and dirty way to determine if you're in the CSS.
 * This only works if we're in VS Mode.
 */
int vsCheckCSS(void)
{
	u32 min_scene;
	sync_before_read((void*)0x00479d20, 0x20);
	min_scene = read32(0x00479d30) & 0x000000ff;
	if (min_scene == 0x00)
		return 1;
	else 
		return 0;
}


/* checkCrash()
 * Reach out into (PPC-addressible) MEM1 and check Melee's frame counter. If
 * the frame counter hasn't been incremented after a certain period of time,
 * this gives some good indication that Melee has crashed.
 */


#define MELEE_CRASH_TIMEOUT	20
extern s32 top_fd;
static u32 crash_ts = 0;
static u32 last_frame = 0;
static u8 crashmsg[] = "CRASHED\x00";
int checkCrash(int socket)
{
	if (TimerDiffSeconds(crash_ts) < MELEE_CRASH_TIMEOUT)
		return 0;

	u32 current_frame = read32(MELEE_FRAME_CTR);
	if (current_frame != last_frame)
	{
		last_frame = current_frame;
		crash_ts = read32(HW_TIMER);
		return 0;
	}

	// When we detect a crash, send a packet to the connected client
	sendto(top_fd, socket, crashmsg, sizeof(crashmsg), 0);
	return 1;
}
