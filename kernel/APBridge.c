/*
 * Nintendont AP Bridge
 *
 * Experimental Archipelago memory bridge for GameCube games
 * running natively through Nintendont on Wii.
 */

#include "global.h"
#include "common.h"
#include "alloc.h"
#include "APBridge.h"

#define APBRIDGE_STACK_SIZE 0x800
#define APBRIDGE_THREAD_PRIORITY 0x78

static u32 APBridge_ThreadID = 0;
static u32 *APBridge_Stack = NULL;

static u32 APBridge_Thread(void *arg)
{
	(void)arg;

	while(1)
	{
		/*
		 * v0.1:
		 * Keep the bridge thread alive without doing anything yet.
		 *
		 * Networking and MEM1 access will be added in later revisions.
		 */
		mdelay(1000);
	}

	return 0;
}

void APBridge_Init(void)
{
	APBridge_Stack = (u32*)malloca(APBRIDGE_STACK_SIZE, 32);

	APBridge_ThreadID = thread_create(
		APBridge_Thread,
		NULL,
		APBridge_Stack,
		APBRIDGE_STACK_SIZE / sizeof(u32),
		APBRIDGE_THREAD_PRIORITY,
		1
	);

	thread_continue(APBridge_ThreadID);
}
