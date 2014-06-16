/*

Nintendont (Kernel) - Playing Gamecubes in Wii mode on a Wii U

Copyright (C) 2013  crediar

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/
#include "global.h"
#include "Patch.h"
#include "PatchCodes.h"

FuncPattern FPatterns[] =
{
	{ 0xCC,			17,     10,     5,      3,      2,	DVDInquiryAsync,			sizeof(DVDInquiryAsync),		"DVDInquiryAsync A",			1,		0 },
	{ 0xC0,			18,     8,      4,      1,      3,	DVDInquiryAsync,			sizeof(DVDInquiryAsync),		"DVDInquiryAsync B",			1,		0 },
	{ 0xC8,			16,     9,      5,      3,      3,	DVDSeekAbsAsyncPrio,		sizeof(DVDSeekAbsAsyncPrio),	"DVDSeekAbsAsyncPrio",			0,		0 },
	{ 0xA8,			10,     4,      4,      6,      3,	(u8*)DVDGetDriveStatus,		sizeof(DVDGetDriveStatus),		"DVDGetDriveStatus",			0,		0 },
//	{ 0xD4,			13,     8,      11,     2,      7,	(u8*)NULL,					0xdead0004,						"AIResetStreamSampleCount",		0,		0 },

//	{ 0x208,        38,     18,     3,      13,     10,	SITransfer,					sizeof(SITransfer),				"__SITransfer",					0,		0 },
	{ 0x158,        26,     22,     5,      13,     2,	ARQPostRequest,				sizeof(ARQPostRequest),			"ARQPostRequest",				0,		0 },
	{ 0xEC,			9,      6,      2,      0,      8,	(u8*)NULL,					0xdead0024,						"ARStartDMA",					0,		0 },
	{ 0x44,			4,      4,      0,      0,      2,	(u8*)NULL,					0xdead0005,						"GXInitTlutObj A",				0,		0 },	// Don't group them, false hit prevents finding real offset
	{ 0x34,			5,      4,      0,      0,      0,	(u8*)NULL,					0xdead0005,						"GXInitTlutObj B",				0,		0 },
	{ 0x1C0,        35,     9,      8,      7,      19,	SIGetType,					sizeof(SIGetType),				"SIGetType",					0,		0 },

	{ 0x168,        22,     10,     7,      6,      10,	SITransfer,					sizeof(SITransfer),				"SITransfer",					0,		0 },
	{ 0x208,        38,     18,     3,     13,      10,	(u8*)NULL,					0xdead0028,						"_SITransfer",					30,		0 },
	{ 0x204,        37,     18,     3,     13,      11,	(u8*)NULL,					0xdead0028,						"_SITransfer",					30,		0 },
	{ 0x2F8,        60,     22,     2,     16,      25,	(u8*)NULL,					0xdead0029,						"CompleteTransfer",				31,		0 },
	{ 0x240,        40,     14,     0,     13,      11,	(u8*)NULL,					0xdead0029,						"CompleteTransfer",				31,		0 },
	{ 0x340,        61,     10,     7,     26,      32,	(u8*)NULL,					0xdead002A,						"SIInterruptHandler",			0,		0 },
	{ 0xB0,         21,     9,      8,     0,       2,	(u8*)NULL,					0xdead002B,						"SIInit",						32,		0 },
	{ 0x70,         13,     8,      2,     0,       2,	(u8*)NULL,					0xdead002B,						"SIInit",						32,		0 },

	{ 0x910,        87,     33,     18,     5,      63,	(u8*)NULL,					0xdead0008,						"__ARChecksize A",				3,		0 },
	{ 0x17F0,       204,    51,     27,     5,      178,(u8*)NULL,					0xdead0008,						"__ARChecksize B",				3,		0 },
	{ 0xEC8,        129,    29,     32,     9,      80, (u8*)NULL,					0xdead0008,						"__ARChecksize C",				3,		0 },
	
	//{ 0x120,        28,     6,      10,     2,      7,	(u8*)NULL,					0xdead000E,						"__OSReadROM",					0,		0 },	

	{ 0xCC,			3,		3,		1,		0,		3,	(u8*)NULL,					0xdead000C,						"C_MTXPerspective",				0,		0 },
	{ 0xC8,			3,      3,      1,      0,      3,	(u8*)NULL,					0xdead000D,						"C_MTXLightPerspective",		0,		0 },
	{ 0x144,        9,      3,      1,      10,     6,	(u8*)NULL,					0xdead0010,						"J3DUClipper::clip()",			0,		0 },	// These are two different functions
	{ 0x2E4,        39,     8,      3,      13,     9,	(u8*)NULL,					0xdead0010,						"J3DUClipper::clip()",			0,		0 },
//	{ 0x94,			0,      0,      0,      0,      0,	(u8*)NULL,					0xdead0010,						"C_MTXOrtho",					0,		0 },

	{ 0xB4,			18,     11,     1,      0,      7, (u8*)NULL,					0xdead0002,						"GCAMSendCMD",					0,		0 },
	{ 0xCC,			20,     16,     5,      0,      3, (u8*)NULL,					0xdead0002,						"GCAMRead",						0,		0 },
	{ 0x9C,			18,     9,      4,      0,      2, (u8*)NULL,					0xdead0002,						"GCAMExecute",					0,		0 },
	{ 0xB0,			19,     12,     4,      0,      2, (u8*)NULL,					0xdead0002,						"GCAMWrite",					0,		0 },
	{ 0x98,			19,     9,      4,      0,      2, (u8*)NULL,					0xdead0002,						"GCAMIdentify",					0,		0 },
	{ 0x54,			10,     2,      2,      0,      2, (u8*)NULL,					0xdead0025,						"GCAMSendCommand",				0,		0 },

	{ 0x10C,        30,     18,     5,      2,      3,	(u8*)NULL,					0xdead0002,						"DVDLowRead A",					5,		0 },
	{ 0xDC,			23,     18,     3,      2,      4,	(u8*)NULL,					0xdead0002,						"DVDLowRead B",					5,		0 },
	{ 0x104,        29,     17,     5,      2,      3,	(u8*)NULL,					0xdead0002,						"DVDLowRead C",					5,		0 },

	{ 0x94,			18,     10,     2,      0,      2,	(u8*)NULL,					0xdead0002,						"DVDLowAudioStream A",			6,		0 },
	{ 0x8C,			16,     12,     1,      0,      2,	(u8*)NULL,					0xdead0002,						"DVDLowAudioStream B",			6,		0 },
	{ 0x88,			18,     8,      2,      0,      2,	(u8*)NULL,					0xdead0002,						"DVDLowRequestAudioStatus",		0,		0 },
	{ 0x98,			19,     8,      2,      1,      3,	(u8*)NULL,					0xdead0002,						"DVDLowAudioBufferConfig",		0,		0 },

	{ 0x308,        40,     18,     10,		23,		17,	(u8*)NULL,					0xdead0026,		"__fwrite A",					7,		0 },
	{ 0x338,        48,     20,     10,		24,		16,	(u8*)NULL,					0xdead0026,		"__fwrite B",					7,		0 },
	{ 0x2D8,        41,     17,     8,		21,		13,	(u8*)NULL,					0xdead0026,		"__fwrite C",					7,		0 },
	{ 0x1FC,        47,     4,      14,		18,		7,	(u8*)NULL,					0xdead0027,		"__fwrite D",					7,		0 },
	//{ 0x308,        40,     18,     10,		23,		17,	patch_fwrite_GC,			sizeof(patch_fwrite_GC),		"__fwrite A",					7,		0 },
	//{ 0x338,        48,     20,     10,		24,		16,	patch_fwrite_GC,			sizeof(patch_fwrite_GC),		"__fwrite B",					7,		0 },
	//{ 0x2D8,        41,     17,     8,		21,		13,	patch_fwrite_GC,			sizeof(patch_fwrite_GC),		"__fwrite C",					7,		0 },

	{ 0x98,			8,      3,      0,      3,      5,	(u8*)NULL,					0xdead000F,						"__GXSetVAT",					0,		0 },

	{ 0x3A8,        86,     13,     27,     17,     24,	(u8*)PADRead,				sizeof(PADRead),				"PADRead A",					8,		0 },
	{ 0x2FC,        73,     8,      23,     16,     15,	(u8*)PADRead,				sizeof(PADRead),				"PADRead B",					8,		0 },
	{ 0x3B0,        87,     13,     27,     17,     25,	(u8*)PADRead,				sizeof(PADRead),				"PADRead C",					8,		0 },
	{ 0x334,        78,     7,      20,     17,     19,	(u8*)PADRead,				sizeof(PADRead),				"PADRead D",					8,		0 },
	{ 0x2A8,        66,     4,      20,     17,     14,	(u8*)PADRead,				sizeof(PADRead),				"PADRead E",					8,		0 },

	{ 0xB4,			11,     5,      5,      3,      5,	(u8*)PADControlMotor,		sizeof(PADControlMotor),		"PADControlMotor A",		9,		0 },
	{ 0xA0,			10,     5,      5,      2,      5,	(u8*)PADControlMotor,		sizeof(PADControlMotor),		"PADControlMotor B",		9,		0 },
	{ 0xB8,			14,     5,      4,      2,      7,	(u8*)PADControlMotor,		sizeof(PADControlMotor),		"PADControlMotor C",		9,		0 },


	{ 0x1F0,        34,     9,      1,      8,      21,	(u8*)NULL,					0xdead0020,						"TCIntrruptHandler A",		28,		0 },
	{ 0x214,        41,     9,      5,      8,      22,	(u8*)NULL,					0xdead0020,						"TCIntrruptHandler B",		28,		0 },
	{ 0x214,        37,     9,      5,      8,      21,	(u8*)NULL,					0xdead0020,						"TCIntrruptHandler C",		28,		0 },
	{ 0xA8,			17,     6,      1,      1,      7,	(u8*)NULL,					0xdead0020,						"TCIntrruptHandler D",		28,		0 },

	{ 0x7C,         10,     3,      0,      1,      7,	(u8*)NULL,					0xdead0020,						"EXIntrruptHandler A",		29,		0 },
	{ 0xC4,			19,     6,      4,      1,      7,	(u8*)NULL,					0xdead0020,						"EXIntrruptHandler B",		29,		0 },
	{ 0xC4,			19,     6,      4,      1,      8,	(u8*)NULL,					0xdead0020,						"EXIntrruptHandler C",		29,		0 },

	{ 0xF0,			17,     7,      5,      5,      7,	EXILock,					sizeof(EXILock),				"EXILock",					20,		0 },
	{ 0xF0,			18,     7,      5,      5,      6,	EXILock,					sizeof(EXILock),				"EXILock",					20,		0 },
	{ 0xD8,			21,     8,      5,      3,      3,	EXILock,					sizeof(EXILock),				"EXIUnlock",				21,		0 },
	{ 0xD8,			21,     8,      5,      3,      2,	EXILock,					sizeof(EXILock),				"EXIUnlock",				21,		0 },
	{ 0x128,        18,     4,      6,      11,     8,	EXISelect,					sizeof(EXISelect),				"EXISelect",				22,		0 },
	{ 0x128,        18,     4,      6,      11,     7,	EXISelect,					sizeof(EXISelect),				"EXISelect",				22,		0 },
	{ 0x258,        36,     8,      5,      12,     32,	EXIImm,						sizeof(EXIImm),					"EXIImm",					23,		0 },
	{ 0x258,        27,     8,      5,      12,     17,	EXIImm,						sizeof(EXIImm),					"EXIImm",					23,		0 },
	{ 0xE8,			17,     7,      5,      2,      5,	(u8*)NULL,					0xdead0021,						"EXIDMA",					24,		0 },
	{ 0xE8,			17,     7,      5,      2,      4,	(u8*)NULL,					0xdead0021,						"EXIDMA",					24,		0 },
	{ 0x234,        39,     3,      3,      12,     19,	EXILock,					sizeof(EXILock),				"EXISync",					25,		0 },
	{ 0x248,        40,     3,      4,      13,     19,	EXILock,					sizeof(EXILock),				"EXISync",					25,		0 },
	{ 0x204,        31,     3,      3,      11,     17,	EXILock,					sizeof(EXILock),				"EXISync",					25,		0 },
	{ 0x234,        35,     3,      3,      12,     17,	EXILock,					sizeof(EXILock),				"EXISync",					25,		0 },
	{ 0x10C,        20,     8,      6,      12,     4,	EXILock,					sizeof(EXILock),				"EXIDeselect",				26,		0 },
	{ 0x10C,        20,     8,      6,      12,     3,	EXILock,					sizeof(EXILock),				"EXIDeselect",				26,		0 },
	{ 0x170,        30,     7,      5,      8,      9,	EXIProbe,					sizeof(EXIProbe),				"__EXIProbe",				27,		0 },
	{ 0x170,        30,     7,      5,      8,      10,	EXIProbe,					sizeof(EXIProbe),				"__EXIProbe",				27,		0 },
	{ 0x164,        30,     4,      5,      8,      10,	EXIProbe,					sizeof(EXIProbe),				"__EXIProbe",				27,		0 },
//	{ 0x378,        69,     11,     26,     20,     20,	EXIGetID,					sizeof(EXIGetID),				"EXIGetID",					0,		0 },
	{ 0xEC,			24,     6,      6,      3,      7,	__CARDReadStatus,			sizeof(__CARDReadStatus),		"__CARDReadStatus",			0,		0 },
	{ 0xA8,			17,     5,      4,      3,      5,	__CARDReadStatus+8,			8,								"__CARDClearStatus",		0,		0 },
	{ 0x1B0,        32,     6,      8,      13,     14,	(u8*)NULL,					0xdead0022,						"__CARDStat A",				0,		0 },
	{ 0x19C,        38,     8,      6,      13,     14,	(u8*)NULL,					0xdead0023,						"__CARDStat B",				0,		0 },
	//{ 0x130,        33,     8,      6,      5,      2,	__CARDReadSegment,	sizeof(__CARDReadSegment),			"__CARDReadSegment",		0,		0 },
	//{ 0x60,			7,      6,      1,      1,      3,	__CARDRead,			sizeof(__CARDRead),					"__CARDRead",				0,		0 },
	//{ 0xDC,			17,     9,      4,      3,      2,	__CARDEraseSector,	sizeof(__CARDEraseSector),			"__CARDEraseSector",		0,		0 },
};
