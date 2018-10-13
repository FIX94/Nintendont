/* SlippiDebug.c
 * Miscellaneous functions to help debug things in PPC-world.
 */

#include "SlippiDebug.h"
#include "net.h"
#include "syscalls.h"

/* "Determining whether or not you're in CSS" - @UnclePunch_:
 * 1. Read the byte at 80479d30 (current major scene ID), and then the byte at
 *    80479d33 (current minor scene ID).
 * 2. Iterate through the major scene table at 803daca4 (0x14-byte members). 
 *    Find the member where (byte at offset 0x1 == current major scene ID) and 
 *    save the minor scene table pointer at offset 0x10.
 * 3. if ((minor scene pointer + (current minor scene ID * 0x18) + 0xC) == 0x8)
 *    then we're currently on the CSS for the current major scene.
 *
 * This seems broken right now. Probably weird cache things. 
 */
static void *majorSceneTable = (void*)MELEE_MAJ_SCENE_TABLE;
int checkCSS(void)
{
	int e;

	sync_before_read((void*)0x00479d20, 0x20);
	u8 currentMajorScene = *(u8*)0x00479d30;
	u8 currentMinorScene = *(u8*)0x00479d33;

	void *minorSceneTable = NULL;
	void *entry = majorSceneTable;
	for (e = 0; e < 46; entry += 0x14) {
		if (*(u8*)(entry + 0xc) == currentMajorScene) {
			minorSceneTable = (void*)(*(u32*)(entry + 0x10));
			break;
		}
		e++;
	}
	if (minorSceneTable == NULL)
		return 0;
	if (*(u8*)(minorSceneTable + (currentMinorScene * 0x18) + 0xc) == 0x8)
		return 1;
	else
		return 0;
}

/* Quick and dirty way to determine if you're in the CSS. This only works if
 * we're in VS Mode. */
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

