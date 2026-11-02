/**
 * Host-side tests for loader/source/MemCardPro.c.
 * The real source is built against the mock libogc headers in tests/mock.
 * To run: make -C loader/tests
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../../common/include/CommonConfig.h"
#include <ogc/card.h>
#include <ogc/exi.h>
#include "MemCardPro.h"

#define MAX_LOG 4096

// One completed transfer on the bus.
typedef struct {
	s32 chan;
	s32 speed;
	u32 mode;
	u32 len;
	u8 data[64];
} Xfer;

// Mock device state for one slot.
typedef struct {
	s32 probe[8];		// CARD_ProbeEx() results. The last one repeats.
	u32 probeCnt, probeIdx, probeCalls;
	u8 id[4];		// Device ID response.
	u32 lockFails;		// Number of EXI_Lock() failures. (-1 == always)
	bool selectFail;
	s32 immFailAfter;	// Fail after this many transfers. (<0 == never)
	u32 selectCalls;
	u32 absentFrom, absentTo;	// ID reads where the device is off the bus.
	u32 idReads;
	bool locked, selected;
} Slot;

static Slot slots[2];
static Xfer xlog[MAX_LOG];
static u32 xcount, sleeps;
static s32 curSpeed;

static void reset(void)
{
	int i;
	memset(slots, 0, sizeof(slots));
	for (i = 0; i < 2; i++) {
		slots[i].probe[0] = CARD_ERROR_NOCARD;
		slots[i].probeCnt = 1;
		slots[i].immFailAfter = -1;
	}
	xcount = 0;
	sleeps = 0;
}

// Add a device that identifies itself the way FlipperMCE does.
static void addDevice(int chan)
{
	static const u8 id[4] = {0x38, 0x42, 0x01, 0x01};
	slots[chan].probe[0] = CARD_ERROR_READY;
	memcpy(slots[chan].id, id, 4);
}

int usleep(useconds_t usec)
{
	(void)usec;
	sleeps++;
	return 0;
}

s32 CARD_ProbeEx(s32 chn, s32 *mem_size, s32 *sect_size)
{
	Slot *s = &slots[chn];
	s32 ret = s->probe[s->probeIdx];
	s->probeCalls++;
	if (s->probeIdx + 1 < s->probeCnt)
		s->probeIdx++;
	return ret;
}

s32 EXI_Lock(s32 nChn, s32 nDev, EXICallback unlockCB)
{
	Slot *s = &slots[nChn];
	assert(nDev == EXI_DEVICE_0);
	assert(unlockCB == NULL);
	assert(!s->locked);
	if (s->lockFails) {
		if (s->lockFails != (u32)-1)
			s->lockFails--;
		return 0;
	}
	s->locked = true;
	return 1;
}

s32 EXI_Unlock(s32 nChn)
{
	assert(slots[nChn].locked);
	assert(!slots[nChn].selected);
	slots[nChn].locked = false;
	return 1;
}

s32 EXI_Select(s32 nChn, s32 nDev, s32 nFrq)
{
	Slot *s = &slots[nChn];
	assert(s->locked);
	assert(!s->selected);
	assert(nDev == EXI_DEVICE_0);
	s->selectCalls++;
	if (s->selectFail)
		return 0;
	s->selected = true;
	curSpeed = nFrq;
	return 1;
}

s32 EXI_Deselect(s32 nChn)
{
	assert(slots[nChn].selected);
	slots[nChn].selected = false;
	return 1;
}

s32 EXI_ImmEx(s32 nChn, void *pData, u32 nLen, u32 nMode)
{
	Slot *s = &slots[nChn];
	Xfer *x;

	assert(s->selected);
	assert(xcount < MAX_LOG);
	assert(nLen <= sizeof(x->data));
	if (s->immFailAfter == 0)
		return 0;
	if (s->immFailAfter > 0)
		s->immFailAfter--;

	x = &xlog[xcount++];
	x->chan = nChn;
	x->speed = curSpeed;
	x->mode = nMode;
	x->len = nLen;
	if (nMode == EXI_WRITE) {
		memcpy(x->data, pData, nLen);
	} else {
		assert(nLen == 4);
		s->idReads++;
		if (s->idReads >= s->absentFrom && s->idReads < s->absentTo)
			memset(pData, 0xFF, nLen);
		else
			memcpy(pData, s->id, nLen);
		memcpy(x->data, pData, nLen);
	}
	return 1;
}

// Build a valid disc header. Bytes that shouldn't be used are poisoned.
static void makeHdr(u8 *hdr, const char *id6, u8 disc, u8 rev, const char *title)
{
	memset(hdr, 0xCC, MCP_DISC_HDR_SIZE);
	memcpy(hdr, id6, 6);
	hdr[0x06] = disc;
	hdr[0x07] = rev;
	hdr[0x1C] = 0xC2; hdr[0x1D] = 0x33; hdr[0x1E] = 0x9F; hdr[0x1F] = 0x3D;
	memset(&hdr[0x20], 0, 64);
	if (title)
		memcpy(&hdr[0x20], title, strlen(title));
}

static u32 countCmd(s32 chan, u8 sub)
{
	u32 i, n = 0;
	for (i = 0; i < xcount; i++) {
		if (xlog[i].chan == chan && xlog[i].mode == EXI_WRITE &&
		    xlog[i].len >= 2 && xlog[i].data[0] == 0x8B && xlog[i].data[1] == sub)
			n++;
	}
	return n;
}

static void assertReleased(void)
{
	assert(!slots[0].locked && !slots[0].selected);
	assert(!slots[1].locked && !slots[1].selected);
}

#define RUN(fn) do { reset(); fn(); printf("PASS  %s\n", #fn); passed++; } while (0)

// These bytes match USB Loader GX's MCP_SetDiskID() in full ID mode.
static void packet_ordinary(void)
{
	static const u8 expected[12] = {0x8B,0x11,'G','A','L','E','0','1','0','0','0','0'};
	u8 hdr[MCP_DISC_HDR_SIZE], cmd[12];
	makeHdr(hdr, "GALE01", 0, 0, "Super Smash Bros Melee");
	assert(MCP_BuildGameIDCmd(hdr, sizeof(hdr), cmd));
	assert(!memcmp(cmd, expected, 12));
}

static void packet_disc_and_revision(void)
{
	static const u8 expected[12] = {0x8B,0x11,'G','M','6','E','0','1','0','1','0','2'};
	u8 hdr[MCP_DISC_HDR_SIZE], cmd[12];
	makeHdr(hdr, "GM6E01", 1, 2, "x");
	assert(MCP_BuildGameIDCmd(hdr, sizeof(hdr), cmd));
	assert(!memcmp(cmd, expected, 12));
}

static void packet_max_hex(void)
{
	u8 hdr[MCP_DISC_HDR_SIZE], cmd[12];
	makeHdr(hdr, "GALE01", 0xFF, 0xAB, "x");
	assert(MCP_BuildGameIDCmd(hdr, sizeof(hdr), cmd));
	assert(!memcmp(&cmd[8], "FFAB", 4));
}

static void packet_rejects_short_header(void)
{
	u8 hdr[MCP_DISC_HDR_SIZE], cmd[12];
	makeHdr(hdr, "GALE01", 0, 0, "x");
	assert(!MCP_BuildGameIDCmd(hdr, MCP_DISC_HDR_SIZE - 1, cmd));
	assert(!MCP_BuildGameIDCmd(hdr, 0, cmd));
}

static void packet_rejects_bad_magic(void)
{
	u8 hdr[MCP_DISC_HDR_SIZE], cmd[12];
	makeHdr(hdr, "GALE01", 0, 0, "x");
	hdr[0x1F] = 0;
	assert(!MCP_BuildGameIDCmd(hdr, sizeof(hdr), cmd));
}

// A CISO container's first bytes, or the zeros left by an unreadable header.
static void packet_rejects_container_and_zero_headers(void)
{
	u8 hdr[MCP_DISC_HDR_SIZE], cmd[12];
	memset(hdr, 0, sizeof(hdr));
	assert(!MCP_BuildGameIDCmd(hdr, sizeof(hdr), cmd));
	memcpy(hdr, "CISO\x00\x00\x20\x00", 8);
	assert(!MCP_BuildGameIDCmd(hdr, sizeof(hdr), cmd));
}

static void packet_rejects_invalid_identifier(void)
{
	static const char bad[4][6] = {
		{'G','A','L',0,'0','1'}, {'G','A',' ','E','0','1'},
		{'G','A','L','E',(char)0xFF,'1'}, {'G','A','L','E','0',0},
	};
	u8 hdr[MCP_DISC_HDR_SIZE], cmd[12];
	u32 i;
	for (i = 0; i < 4; i++) {
		makeHdr(hdr, "GALE01", 0, 0, "x");
		memcpy(hdr, bad[i], 6);
		assert(!MCP_BuildGameIDCmd(hdr, sizeof(hdr), cmd));
	}
}

static void title_empty(void)
{
	static const char zero[MCP_TITLE_SIZE] = {0};
	u8 hdr[MCP_DISC_HDR_SIZE];
	char title[MCP_TITLE_SIZE];
	makeHdr(hdr, "GALE01", 0, 0, NULL);
	memset(title, 0xCC, sizeof(title));
	assert(!MCP_BuildTitle(hdr, sizeof(hdr), title));
	assert(!memcmp(title, zero, sizeof(title)));
}

static void title_short_is_zero_padded(void)
{
	u8 hdr[MCP_DISC_HDR_SIZE];
	char title[MCP_TITLE_SIZE];
	u32 i;
	makeHdr(hdr, "GALE01", 0, 0, "Melee");
	memset(title, 0xCC, sizeof(title));
	assert(MCP_BuildTitle(hdr, sizeof(hdr), title));
	assert(!strcmp(title, "Melee"));
	for (i = 5; i < sizeof(title); i++)
		assert(title[i] == 0);
}

static void title_unterminated_is_truncated(void)
{
	u8 hdr[MCP_DISC_HDR_SIZE];
	char title[MCP_TITLE_SIZE];
	makeHdr(hdr, "GALE01", 0, 0, NULL);
	memset(&hdr[0x20], 'A', 64);
	assert(MCP_BuildTitle(hdr, sizeof(hdr), title));
	assert(strlen(title) == 63);
}

static void title_non_ascii_is_preserved(void)
{
	static const char sjis[] = "\x83\x5B\x83\x8B\x83\x5F";
	u8 hdr[MCP_DISC_HDR_SIZE];
	char title[MCP_TITLE_SIZE];
	makeHdr(hdr, "GZLJ01", 0, 0, sjis);
	assert(MCP_BuildTitle(hdr, sizeof(hdr), title));
	assert(!strcmp(title, sjis));
}

static void title_rejects_short_header(void)
{
	u8 hdr[MCP_DISC_HDR_SIZE];
	char title[MCP_TITLE_SIZE];
	makeHdr(hdr, "GALE01", 0, 0, "x");
	assert(!MCP_BuildTitle(hdr, 0x20, title));
}

static void slots_real_cards(void)
{
	assert(MCP_GetEligibleSlots(0, false, false, false) == (MCP_SLOT_A | MCP_SLOT_B));
}

static void slots_emulation_and_hybrid(void)
{
	assert(MCP_GetEligibleSlots(NIN_CFG_MEMCARDEMU, false, false, false) == 0);
	assert(MCP_GetEligibleSlots(NIN_CFG_MEMCARDEMU | NIN_CFG_HYBRID_EXI, false, false, false) == MCP_SLOT_B);
	// Hybrid has no effect without memory card emulation.
	assert(MCP_GetEligibleSlots(NIN_CFG_HYBRID_EXI, false, false, false) == (MCP_SLOT_A | MCP_SLOT_B));
}

static void slots_wiiu_and_triforce(void)
{
	assert(MCP_GetEligibleSlots(0, true, false, false) == 0);
	assert(MCP_GetEligibleSlots(0, false, true, false) == 0);
	assert(MCP_GetEligibleSlots(NIN_CFG_MEMCARDEMU | NIN_CFG_HYBRID_EXI, true, false, false) == 0);
}

static void slots_gecko_and_debugger(void)
{
	assert(MCP_GetEligibleSlots(0, false, false, true) == MCP_SLOT_A);
	assert(MCP_GetEligibleSlots(NIN_CFG_DEBUGGER, false, false, false) == MCP_SLOT_A);
	assert(MCP_GetEligibleSlots(NIN_CFG_MEMCARDEMU | NIN_CFG_HYBRID_EXI, false, false, true) == 0);
}

static void flow_compatible_device(void)
{
	static const u8 expected[12] = {0x8B,0x11,'G','A','L','E','0','1','0','1','0','2'};
	u8 hdr[MCP_DISC_HDR_SIZE];
	addDevice(0);
	makeHdr(hdr, "GALE01", 1, 2, "Melee");
	MCP_SendGameInfo(hdr, sizeof(hdr), MCP_SLOT_A);

	// Identify at 1 MHz, then send the ID and then the title at 16 MHz.
	assert(xlog[0].speed == EXI_SPEED1MHZ && xlog[0].len == 2 &&
	       xlog[0].data[0] == 0x8B && xlog[0].data[1] == 0x00);
	assert(xlog[1].speed == EXI_SPEED1MHZ && xlog[1].mode == EXI_READ);
	assert(xlog[2].speed == EXI_SPEED16MHZ && xlog[2].len == 12);
	assert(!memcmp(xlog[2].data, expected, 12));
	assert(xlog[3].speed == EXI_SPEED16MHZ && xlog[3].len == 2 && xlog[3].data[1] == 0x13);
	assert(xlog[4].speed == EXI_SPEED16MHZ && xlog[4].len == 64 &&
	       !strcmp((const char*)xlog[4].data, "Melee"));
	assert(countCmd(0, 0x11) == 1 && countCmd(0, 0x13) == 1);
	assertReleased();
}

static void flow_both_slots(void)
{
	u8 hdr[MCP_DISC_HDR_SIZE];
	addDevice(0);
	addDevice(1);
	makeHdr(hdr, "GALE01", 0, 0, "x");
	MCP_SendGameInfo(hdr, sizeof(hdr), MCP_SLOT_A | MCP_SLOT_B);
	assert(countCmd(0, 0x11) == 1 && countCmd(1, 0x11) == 1);
	assertReleased();
}

static void flow_ineligible_slot_is_not_touched(void)
{
	u8 hdr[MCP_DISC_HDR_SIZE];
	addDevice(0);
	addDevice(1);
	makeHdr(hdr, "GALE01", 0, 0, "x");
	MCP_SendGameInfo(hdr, sizeof(hdr), MCP_SLOT_B);
	assert(slots[0].probeCalls == 0 && countCmd(0, 0x00) == 0);
	assert(countCmd(1, 0x11) == 1);

	reset();
	addDevice(0);
	MCP_SendGameInfo(hdr, sizeof(hdr), 0);
	assert(xcount == 0 && slots[0].probeCalls == 0);
}

static void flow_empty_slot(void)
{
	u8 hdr[MCP_DISC_HDR_SIZE];
	makeHdr(hdr, "GALE01", 0, 0, "x");
	MCP_SendGameInfo(hdr, sizeof(hdr), MCP_SLOT_A | MCP_SLOT_B);
	assert(xcount == 0);
	assertReleased();
}

// Microphones and other non-card devices get no 0x8B commands at all.
static void flow_non_card_device(void)
{
	u8 hdr[MCP_DISC_HDR_SIZE];
	slots[0].probe[0] = CARD_ERROR_WRONGDEVICE;
	makeHdr(hdr, "GALE01", 0, 0, "x");
	MCP_SendGameInfo(hdr, sizeof(hdr), MCP_SLOT_A);
	assert(xcount == 0);
}

// An ordinary memory card sees the ID query, but never the game ID.
static void flow_wrong_device_id(void)
{
	u8 hdr[MCP_DISC_HDR_SIZE];
	addDevice(0);
	memset(slots[0].id, 0xFF, 4);
	makeHdr(hdr, "GALE01", 0, 0, "x");
	MCP_SendGameInfo(hdr, sizeof(hdr), MCP_SLOT_A);
	assert(countCmd(0, 0x00) > 0);
	assert(countCmd(0, 0x11) == 0 && countCmd(0, 0x13) == 0);
	assertReleased();
}

static void flow_invalid_header_sends_nothing(void)
{
	u8 hdr[MCP_DISC_HDR_SIZE];
	addDevice(0);
	memset(hdr, 0, sizeof(hdr));
	MCP_SendGameInfo(hdr, sizeof(hdr), MCP_SLOT_A);
	assert(xcount == 0 && slots[0].probeCalls == 0);
}

static void flow_empty_title_is_not_sent(void)
{
	u8 hdr[MCP_DISC_HDR_SIZE];
	addDevice(0);
	makeHdr(hdr, "GALE01", 0, 0, NULL);
	MCP_SendGameInfo(hdr, sizeof(hdr), MCP_SLOT_A);
	assert(countCmd(0, 0x11) == 1 && countCmd(0, 0x13) == 0);
}

// CARD_ProbeEx() really does this right after a card is first detected.
static void flow_transient_probe_busy(void)
{
	u8 hdr[MCP_DISC_HDR_SIZE];
	addDevice(0);
	slots[0].probe[0] = CARD_ERROR_BUSY;
	slots[0].probe[1] = CARD_ERROR_BUSY;
	slots[0].probe[2] = CARD_ERROR_READY;
	slots[0].probeCnt = 3;
	makeHdr(hdr, "GALE01", 0, 0, "x");
	MCP_SendGameInfo(hdr, sizeof(hdr), MCP_SLOT_A);
	assert(slots[0].probeCalls == 3 && countCmd(0, 0x11) == 1);
}

static void flow_permanent_probe_busy_terminates(void)
{
	u8 hdr[MCP_DISC_HDR_SIZE];
	slots[0].probe[0] = CARD_ERROR_BUSY;
	makeHdr(hdr, "GALE01", 0, 0, "x");
	MCP_SendGameInfo(hdr, sizeof(hdr), MCP_SLOT_A);
	assert(slots[0].probeCalls <= 16 && xcount == 0);
}

static void flow_transient_lock_failure(void)
{
	u8 hdr[MCP_DISC_HDR_SIZE];
	addDevice(0);
	slots[0].lockFails = 3;
	makeHdr(hdr, "GALE01", 0, 0, "x");
	MCP_SendGameInfo(hdr, sizeof(hdr), MCP_SLOT_A);
	assert(countCmd(0, 0x11) == 1);
	assertReleased();
}

static void flow_permanent_lock_failure_terminates(void)
{
	u8 hdr[MCP_DISC_HDR_SIZE];
	addDevice(0);
	slots[0].lockFails = (u32)-1;
	makeHdr(hdr, "GALE01", 0, 0, "x");
	MCP_SendGameInfo(hdr, sizeof(hdr), MCP_SLOT_A);
	assert(xcount == 0 && sleeps <= 16);
	assertReleased();
}

static void flow_select_failure_unlocks(void)
{
	u8 hdr[MCP_DISC_HDR_SIZE];
	addDevice(0);
	slots[0].selectFail = true;
	makeHdr(hdr, "GALE01", 0, 0, "x");
	MCP_SendGameInfo(hdr, sizeof(hdr), MCP_SLOT_A);
	assert(xcount == 0 && slots[0].selectCalls <= 16);
	assertReleased();
}

// Fail at every possible transfer. Nothing may stay locked or selected.
static void flow_transfer_failure_at_each_step(void)
{
	u8 hdr[MCP_DISC_HDR_SIZE];
	s32 step;
	makeHdr(hdr, "GALE01", 0, 0, "x");
	for (step = 0; step < 6; step++) {
		reset();
		addDevice(0);
		slots[0].immFailAfter = step;
		MCP_SendGameInfo(hdr, sizeof(hdr), MCP_SLOT_A);
		assertReleased();
	}
}

static void flow_failure_does_not_stop_other_slot(void)
{
	u8 hdr[MCP_DISC_HDR_SIZE];
	addDevice(0);
	addDevice(1);
	slots[0].selectFail = true;
	makeHdr(hdr, "GALE01", 0, 0, "x");
	MCP_SendGameInfo(hdr, sizeof(hdr), MCP_SLOT_A | MCP_SLOT_B);
	assert(countCmd(1, 0x11) == 1);
	assertReleased();
}

// The device leaves the bus to switch cards, then comes back after 1s.
static void flow_waits_for_card_switch(void)
{
	u8 hdr[MCP_DISC_HDR_SIZE];
	addDevice(0);
	slots[0].absentFrom = 4;
	slots[0].absentTo = 54;
	makeHdr(hdr, "GALE01", 0, 0, "x");
	MCP_SendGameInfo(hdr, sizeof(hdr), MCP_SLOT_A);
	assert(slots[0].idReads == 54);
	assertReleased();
}

// The firmware doesn't switch if this card is already selected.
static void flow_same_game_does_not_wait_long(void)
{
	u8 hdr[MCP_DISC_HDR_SIZE];
	addDevice(0);
	makeHdr(hdr, "GALE01", 0, 0, "x");
	MCP_SendGameInfo(hdr, sizeof(hdr), MCP_SLOT_A);
	assert(sleeps <= 25);
}

static void flow_send_twice_is_identical(void)
{
	u8 hdr[MCP_DISC_HDR_SIZE];
	u8 first[12];
	addDevice(0);
	makeHdr(hdr, "GALE01", 0, 0, "x");
	MCP_SendGameInfo(hdr, sizeof(hdr), MCP_SLOT_A);
	memcpy(first, xlog[2].data, 12);
	xcount = 0;
	MCP_SendGameInfo(hdr, sizeof(hdr), MCP_SLOT_A);
	assert(!memcmp(first, xlog[2].data, 12));
	assertReleased();
}

// The device never comes back. The game must still launch.
static void flow_switch_timeout_terminates(void)
{
	u8 hdr[MCP_DISC_HDR_SIZE];
	addDevice(0);
	slots[0].absentFrom = 4;
	slots[0].absentTo = (u32)-1;
	makeHdr(hdr, "GALE01", 0, 0, "x");
	MCP_SendGameInfo(hdr, sizeof(hdr), MCP_SLOT_A);
	assert(sleeps <= 25 + 750);
	assertReleased();
}

int main(void)
{
	int passed = 0;

	RUN(packet_ordinary);
	RUN(packet_disc_and_revision);
	RUN(packet_max_hex);
	RUN(packet_rejects_short_header);
	RUN(packet_rejects_bad_magic);
	RUN(packet_rejects_container_and_zero_headers);
	RUN(packet_rejects_invalid_identifier);
	RUN(title_empty);
	RUN(title_short_is_zero_padded);
	RUN(title_unterminated_is_truncated);
	RUN(title_non_ascii_is_preserved);
	RUN(title_rejects_short_header);
	RUN(slots_real_cards);
	RUN(slots_emulation_and_hybrid);
	RUN(slots_wiiu_and_triforce);
	RUN(slots_gecko_and_debugger);
	RUN(flow_compatible_device);
	RUN(flow_both_slots);
	RUN(flow_ineligible_slot_is_not_touched);
	RUN(flow_empty_slot);
	RUN(flow_non_card_device);
	RUN(flow_wrong_device_id);
	RUN(flow_invalid_header_sends_nothing);
	RUN(flow_empty_title_is_not_sent);
	RUN(flow_transient_probe_busy);
	RUN(flow_permanent_probe_busy_terminates);
	RUN(flow_transient_lock_failure);
	RUN(flow_permanent_lock_failure_terminates);
	RUN(flow_select_failure_unlocks);
	RUN(flow_transfer_failure_at_each_step);
	RUN(flow_failure_does_not_stop_other_slot);
	RUN(flow_waits_for_card_switch);
	RUN(flow_same_game_does_not_wait_long);
	RUN(flow_send_twice_is_identical);
	RUN(flow_switch_timeout_terminates);

	printf("%d tests passed\n", passed);
	return 0;
}
