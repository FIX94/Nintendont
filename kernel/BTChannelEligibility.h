#ifndef _BT_CHANNEL_ELIGIBILITY_H_
#define _BT_CHANNEL_ELIGIBILITY_H_

static inline u8 BTGenericReportActivatesChannel(u8 transfer_type,
	const u8 *report, u16 length)
{
	return transfer_type == 0x3d && report != 0 && length >= 11 &&
		report[0] == 0x3d;
}

#endif
