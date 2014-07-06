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

enum
{
	FCODES								= 0xdead0000,
	FCODE_ARStartDMA,
	FCODE_AIResetStreamSampleCount,
	FCODE_Return,
										//__AI_set_stream_sample_rate
	FCODE_GXInitTlutObj,
										//GXInitTlutObj_A,
										//GXInitTlutObj_B,
	FCODE__SITransfer, 
										//_SITransfer_A,
										//_SITransfer_B,
	FCODE_CompleteTransfer,
										//CompleteTransfer_A,
										//CompleteTransfer_B,
	FCODE_SIInit,
										//SIInit_A,
										//SIInit_B,
										//SIInit_C,
	FCODE_SIEnablePollingInterrupt,
	FCODE___ARChecksize_A,
	FCODE___ARChecksize_B,
	FCODE___ARChecksize_C,
	FCODE___OSReadROM,
	FCODE_C_MTXPerspective,
	FCODE_C_MTXLightPerspective,
	FCODE_J3DUClipper_clip,
	//FCODE_C_MTXOrtho,
	FCODE_GCAMSendCommand,
	FCODE_PatchFunc,
										//GCAMSendCMD,	
										//GCAMRead,	
										//GCAMExecute,	
										//GCAMWrite,	
										//GCAMIdentify,
										//DVDLowRead_A,
										//DVDLowRead_B,	
										//DVDLowRead_C,	
										//DVDLowAudioStream_A,	
										//DVDLowAudioStream_B,	
										//DVDLowRequestAudioStatus,	
										//DVDLowAudioBufferConfig,
	FCODE___fwrite,
										//__fwrite_A,	
										//__fwrite_B,	
										//__fwrite_C,
	FCODE___fwrite_D,
	FCODE___GXSetVAT,
	FCODE_EXIIntrruptHandler,
										//TCIntrruptHandler_A,	
										//TCIntrruptHandler_B,	
										//TCIntrruptHandler_C,	
										//TCIntrruptHandler_D,	
										//EXIntrruptHandler_A,	
										//EXIntrruptHandler_B,	
										//EXIntrruptHandler_C,
	FCODE_SIInterruptHandler,
										//SIInterruptHandler_A,	
										//SIInterruptHandler_B,
	FCODE_PI_FIFO_WP_A,
	FCODE_PI_FIFO_WP_B,
	FCODE_PI_FIFO_WP_C,
	FCODE_PI_FIFO_WP_D,
	FCODE_PI_FIFO_WP_E,
	FCODE_EXIDMA,
	FCODE___CARDStat_A,
	FCODE___CARDStat_B,
} FPatternCodes;

enum
{
	FGROUP_NONE				= 0x0,
	FGROUP_DVDInquiryAsync,
	FGROUP__SITransfer,
	FGROUP_CompleteTransfer,
	FGROUP_SIInit,
	FGROUP___ARChecksize,
	FGROUP_DVDLowRead,
	FGROUP_DVDLowAudioStream,
	FGROUP___fwrite,
	FGROUP_PADRead,
	FGROUP_PADControlMotor,
	FGROUP_TCIntrruptHandler,
	FGROUP_EXIntrruptHandler,
	FGROUP_SIInterruptHandler,
	FGROUP_EXILock,
	FGROUP_EXIUnlock,
	FGROUP_EXISelect,
	FGROUP_EXIImm,
	FGROUP_EXIDMA,
	FGROUP_EXISync,
	FGROUP_EXIDeselect,
	FGROUP___EXIProbe,
	FGROUP_PI_FIFO_WP_A,
} FPatternGroups;

FuncPattern FPatterns[] =
{
	{   0xCC,   17,    10,     5,     3,     2,	DVDInquiryAsync,		sizeof(DVDInquiryAsync),		"DVDInquiryAsync A",			FGROUP_DVDInquiryAsync,		0 },
	{   0xC0,   18,     8,     4,     1,     3,	DVDInquiryAsync,		sizeof(DVDInquiryAsync),		"DVDInquiryAsync B",			FGROUP_DVDInquiryAsync,		0 },
	{   0xC8,   16,     9,     5,     3,     3,	DVDSeekAbsAsyncPrio,	sizeof(DVDSeekAbsAsyncPrio),	"DVDSeekAbsAsyncPrio",			FGROUP_NONE,				0 },
	{   0xA8,   10,     4,     4,     6,     3,	(u8*)DVDGetDriveStatus,	sizeof(DVDGetDriveStatus),		"DVDGetDriveStatus",			FGROUP_NONE,				0 },
//	{   0xD4,   13,     8,    11,     2,     7,	(u8*)NULL,				FCODE_AIResetStreamSampleCount,	"AIResetStreamSampleCount",		FGROUP_NONE,				0 },

//	{  0x208,   38,    18,     3,    13,    10,	SITransfer,				sizeof(SITransfer),				"__SITransfer",					FGROUP_NONE,				0 },
	{  0x158,   26,    22,     5,    13,     2,	ARQPostRequest,			sizeof(ARQPostRequest),			"ARQPostRequest",				FGROUP_NONE,				0 },
	{   0xEC,    9,     6,     2,     0,     8,	(u8*)NULL,				FCODE_ARStartDMA,				"ARStartDMA",					FGROUP_NONE,				0 },
	{   0x44,    4,     4,     0,     0,     2,	(u8*)NULL,				FCODE_GXInitTlutObj,			"GXInitTlutObj A",				FGROUP_NONE,				0 },	// Don't group them, false hit prevents finding real offset
	{   0x34,    5,     4,     0,     0,     0,	(u8*)NULL,				FCODE_GXInitTlutObj,			"GXInitTlutObj B",				FGROUP_NONE,				0 },
	{  0x1C0,   35,     9,     8,     7,    19,	SIGetType,				sizeof(SIGetType),				"SIGetType",					FGROUP_NONE,				0 },

	{  0x168,   22,    10,     7,     6,    10,	SITransfer,				sizeof(SITransfer),				"SITransfer",					FGROUP_NONE,				0 },
	{  0x208,   38,    18,     3,    13,    10,	(u8*)NULL,				FCODE__SITransfer,				"_SITransfer A",				FGROUP__SITransfer,			0 },
	{  0x204,   37,    18,     3,    13,    11,	(u8*)NULL,				FCODE__SITransfer,				"_SITransfer B",				FGROUP__SITransfer,			0 },
	{  0x2F8,   60,    22,     2,    16,    25,	(u8*)NULL,				FCODE_CompleteTransfer,			"CompleteTransfer A",			FGROUP_CompleteTransfer,	0 },
	{  0x240,   40,    14,     0,    13,    11,	(u8*)NULL,				FCODE_CompleteTransfer,			"CompleteTransfer B",			FGROUP_CompleteTransfer,	0 },
	{   0xB0,   21,     9,     8,     0,     2,	(u8*)NULL,				FCODE_SIInit,					"SIInit A",						FGROUP_SIInit,				0 },
	{   0x70,   13,     8,     2,     0,     2,	(u8*)NULL,				FCODE_SIInit,					"SIInit B",						FGROUP_SIInit,				0 },
	{   0x90,   17,     8,     6,     0,     2,	(u8*)NULL,				FCODE_SIInit,					"SIInit C",						FGROUP_SIInit,				0 },
	{   0xA0,   20,     8,     7,     0,     2,	(u8*)NULL,				FCODE_SIInit,					"SIInit D",						FGROUP_SIInit,				0 },
	{   0x94,    8,    10,     2,     4,     2,	(u8*)NULL,				FCODE_SIEnablePollingInterrupt,	"SIEnablePollingInterrupt",		FGROUP_NONE,				0 },

	{  0x910,   87,    33,    18,     5,    63,	(u8*)NULL,				FCODE___ARChecksize_A,			"__ARChecksize A",				FGROUP___ARChecksize,		0 },
	{ 0x17F0,  204,    51,    27,     5,   178,	(u8*)NULL,				FCODE___ARChecksize_B,			"__ARChecksize B",				FGROUP___ARChecksize,		0 },
	{  0xEC8,  129,    29,    32,     9,    80,	(u8*)NULL,				FCODE___ARChecksize_C,			"__ARChecksize C",				FGROUP___ARChecksize,		0 },
	
//	{  0x120,   28,     6,    10,     2,     7,	(u8*)NULL,				FCODE___OSReadROM,				"__OSReadROM",					FGROUP_NONE,				0 },

	{   0xCC,    3,     3,     1,     0,     3,	(u8*)NULL,				FCODE_C_MTXPerspective,			"C_MTXPerspective",				FGROUP_NONE,				0 },
	{   0xC8,    3,     3,     1,     0,     3,	(u8*)NULL,				FCODE_C_MTXLightPerspective,	"C_MTXLightPerspective",		FGROUP_NONE,				0 },
	{  0x144,    9,     3,     1,    10,     6,	(u8*)NULL,				FCODE_J3DUClipper_clip,			"J3DUClipper::clip()",			FGROUP_NONE,				0 },	// These are two different functions
	{  0x2E4,   39,     8,     3,    13,     9,	(u8*)NULL,				FCODE_J3DUClipper_clip,			"J3DUClipper::clip()",			FGROUP_NONE,				0 },
//	{   0x94,    0,     0,     0,     0,     0,	(u8*)NULL,				FCODE_J3DUClipper_clip,			"C_MTXOrtho",					FGROUP_NONE,				0 },

	{   0xB4,   18,    11,     1,     0,     7,	(u8*)NULL,				FCODE_PatchFunc,				"GCAMSendCMD",					FGROUP_NONE,				0 },
	{   0xCC,   20,    16,     5,     0,     3,	(u8*)NULL,				FCODE_PatchFunc,				"GCAMRead",						FGROUP_NONE,				0 },
	{   0x9C,   18,     9,     4,     0,     2,	(u8*)NULL,				FCODE_PatchFunc,				"GCAMExecute",					FGROUP_NONE,				0 },
	{   0xB0,   19,    12,     4,     0,     2,	(u8*)NULL,				FCODE_PatchFunc,				"GCAMWrite",					FGROUP_NONE,				0 },
	{   0x98,   19,     9,     4,     0,     2,	(u8*)NULL,				FCODE_PatchFunc,				"GCAMIdentify",					FGROUP_NONE,				0 },
	{   0x54,   10,     2,     2,     0,     2,	(u8*)NULL,				FCODE_GCAMSendCommand,			"GCAMSendCommand",				FGROUP_NONE,				0 },

	{  0x10C,   30,    18,     5,     2,     3,	(u8*)NULL,				FCODE_PatchFunc,				"DVDLowRead A",					FGROUP_DVDLowRead,			0 },
	{   0xDC,   23,    18,     3,     2,     4,	(u8*)NULL,				FCODE_PatchFunc,				"DVDLowRead B",					FGROUP_DVDLowRead,			0 },
	{  0x104,   29,    17,     5,     2,     3,	(u8*)NULL,				FCODE_PatchFunc,				"DVDLowRead C",					FGROUP_DVDLowRead,			0 },

	{   0x94,   18,    10,     2,     0,     2,	(u8*)NULL,				FCODE_PatchFunc,				"DVDLowAudioStream A",			FGROUP_DVDLowAudioStream,	0 },
	{   0x8C,   16,    12,     1,     0,     2,	(u8*)NULL,				FCODE_PatchFunc,				"DVDLowAudioStream B",			FGROUP_DVDLowAudioStream,	0 },
	{   0x88,   18,     8,     2,     0,     2,	(u8*)NULL,				FCODE_PatchFunc,				"DVDLowRequestAudioStatus",		FGROUP_NONE,				0 },
	{   0x98,   19,     8,     2,     1,     3,	(u8*)NULL,				FCODE_PatchFunc,				"DVDLowAudioBufferConfig",		FGROUP_NONE,				0 },

	{  0x308,   40,    18,    10,    23,    17,	(u8*)NULL,				FCODE___fwrite,					"__fwrite A",					FGROUP___fwrite,			0 },
	{  0x338,   48,    20,    10,    24,    16,	(u8*)NULL,				FCODE___fwrite,					"__fwrite B",					FGROUP___fwrite,			0 },
	{  0x2D8,   41,    17,     8,    21,    13,	(u8*)NULL,				FCODE___fwrite,					"__fwrite C",					FGROUP___fwrite,			0 },
//	{  0x1FC,   47,     4,    14,    18,     7,	(u8*)NULL,				FCODE___fwrite_D,				"__fwrite D",					FGROUP___fwrite,			0 },

	{   0x98,    8,     3,     0,     3,     5,	(u8*)NULL,				FCODE___GXSetVAT,				"__GXSetVAT",					FGROUP_NONE,				0 },

	{  0x3A8,   86,    13,    27,    17,    24,	(u8*)PADRead,			sizeof(PADRead),				"PADRead A",					FGROUP_PADRead,				0 },
	{  0x2FC,   73,     8,    23,    16,    15,	(u8*)PADRead,			sizeof(PADRead),				"PADRead B",					FGROUP_PADRead,				0 },
	{  0x3B0,   87,    13,    27,    17,    25,	(u8*)PADRead,			sizeof(PADRead),				"PADRead C",					FGROUP_PADRead,				0 },
	{  0x334,   78,     7,    20,    17,    19,	(u8*)PADRead,			sizeof(PADRead),				"PADRead D",					FGROUP_PADRead,				0 },
	{  0x2A8,   66,     4,    20,    17,    14,	(u8*)PADRead,			sizeof(PADRead),				"PADRead E",					FGROUP_PADRead,				0 },

	{   0xB4,   11,     5,     5,     3,     5,	(u8*)PADControlMotor,	sizeof(PADControlMotor),		"PADControlMotor A",			FGROUP_PADControlMotor,		0 },
	{   0xA0,   10,     5,     5,     2,     5,	(u8*)PADControlMotor,	sizeof(PADControlMotor),		"PADControlMotor B",			FGROUP_PADControlMotor,		0 },
	{   0xB8,   14,     5,     4,     2,     7,	(u8*)PADControlMotor,	sizeof(PADControlMotor),		"PADControlMotor C",			FGROUP_PADControlMotor,		0 },

	{  0x1F0,   34,     9,     1,     8,    21,	(u8*)NULL,				FCODE_EXIIntrruptHandler,		"TCIntrruptHandler A",			FGROUP_TCIntrruptHandler,	0 },
	{  0x214,   41,     9,     5,     8,    22,	(u8*)NULL,				FCODE_EXIIntrruptHandler,		"TCIntrruptHandler B",			FGROUP_TCIntrruptHandler,	0 },
	{  0x214,   37,     9,     5,     8,    21,	(u8*)NULL,				FCODE_EXIIntrruptHandler,		"TCIntrruptHandler C",			FGROUP_TCIntrruptHandler,	0 },
	{   0xA8,   17,     6,     1,     1,     7,	(u8*)NULL,				FCODE_EXIIntrruptHandler,		"TCIntrruptHandler D",			FGROUP_TCIntrruptHandler,	0 },

	{   0x7C,   10,     3,     0,     1,     7,	(u8*)NULL,				FCODE_EXIIntrruptHandler,		"EXIntrruptHandler A",			FGROUP_EXIntrruptHandler,	0 },
	{   0xC4,   19,     6,     4,     1,     7,	(u8*)NULL,				FCODE_EXIIntrruptHandler,		"EXIntrruptHandler B",			FGROUP_EXIntrruptHandler,	0 },
	{   0xC4,   19,     6,     4,     1,     8,	(u8*)NULL,				FCODE_EXIIntrruptHandler,		"EXIntrruptHandler C",			FGROUP_EXIntrruptHandler,	0 },

	{  0x340,   61,    10,     7,    26,    32,	(u8*)NULL,				FCODE_SIInterruptHandler,		"SIInterruptHandler A",			FGROUP_SIInterruptHandler,	0 },
	{  0x114,   21,     4,     4,     5,    11,	(u8*)NULL,				FCODE_SIInterruptHandler,		"SIInterruptHandler B",			FGROUP_SIInterruptHandler,	0 },

	{  0x10C,   27,    11,     8,     2,     4,	(u8*)NULL,				FCODE_PI_FIFO_WP_A,				"PI_FIFO_WP A A",				FGROUP_PI_FIFO_WP_A,		0 },
	{  0x10C,   27,    11,     7,     2,     5,	(u8*)NULL,				FCODE_PI_FIFO_WP_A,				"PI_FIFO_WP A B",				FGROUP_PI_FIFO_WP_A,		0 },
	{   0xD8,   20,    11,     3,     3,     6,	(u8*)NULL,				FCODE_PI_FIFO_WP_B,				"PI_FIFO_WP B",					FGROUP_NONE,				0 },
	{   0x94,   14,     7,     0,     2,     4,	(u8*)NULL,				FCODE_PI_FIFO_WP_C,				"PI_FIFO_WP C",					FGROUP_NONE,				0 },
	{   0xC4,   19,    10,     2,     3,     5,	(u8*)NULL,				FCODE_PI_FIFO_WP_D,				"PI_FIFO_WP D",					FGROUP_NONE,				0 },
	{   0xC0,   21,     7,     6,     2,     3,	(u8*)NULL,				FCODE_PI_FIFO_WP_E,				"PI_FIFO_WP E",					FGROUP_NONE,				0 },

	{   0xF0,   17,     7,     5,     5,     7,	EXILock,				sizeof(EXILock),				"EXILock",						FGROUP_EXILock,				0 },
	{   0xF0,   18,     7,     5,     5,     6,	EXILock,				sizeof(EXILock),				"EXILock",						FGROUP_EXILock,				0 },
	{   0xD8,   21,     8,     5,     3,     3,	EXILock,				sizeof(EXILock),				"EXIUnlock",					FGROUP_EXIUnlock,			0 },
	{   0xD8,   21,     8,     5,     3,     2,	EXILock,				sizeof(EXILock),				"EXIUnlock",					FGROUP_EXIUnlock,			0 },
	{  0x128,   18,     4,     6,    11,     8,	EXISelect,				sizeof(EXISelect),				"EXISelect",					FGROUP_EXISelect,			0 },
	{  0x128,   18,     4,     6,    11,     7,	EXISelect,				sizeof(EXISelect),				"EXISelect",					FGROUP_EXISelect,			0 },
	{  0x258,   36,     8,     5,    12,    32,	EXIImm,					sizeof(EXIImm),					"EXIImm",						FGROUP_EXIImm,				0 },
	{  0x258,   27,     8,     5,    12,    17,	EXIImm,					sizeof(EXIImm),					"EXIImm",						FGROUP_EXIImm,				0 },
	{   0xE8,   17,     7,     5,     2,     5,	(u8*)NULL,				FCODE_EXIDMA,					"EXIDMA",						FGROUP_EXIDMA,				0 },
	{   0xE8,   17,     7,     5,     2,     4,	(u8*)NULL,				FCODE_EXIDMA,					"EXIDMA",						FGROUP_EXIDMA,				0 },
	{  0x234,   39,     3,     3,    12,    19,	EXILock,				sizeof(EXILock),				"EXISync",						FGROUP_EXISync,				0 },
	{  0x248,   40,     3,     4,    13,    19,	EXILock,				sizeof(EXILock),				"EXISync",						FGROUP_EXISync,				0 },
	{  0x204,   31,     3,     3,    11,    17,	EXILock,				sizeof(EXILock),				"EXISync",						FGROUP_EXISync,				0 },
	{  0x234,   35,     3,     3,    12,    17,	EXILock,				sizeof(EXILock),				"EXISync",						FGROUP_EXISync,				0 },
	{  0x10C,   20,     8,     6,    12,     4,	EXILock,				sizeof(EXILock),				"EXIDeselect",					FGROUP_EXIDeselect,			0 },
	{  0x10C,   20,     8,     6,    12,     3,	EXILock,				sizeof(EXILock),				"EXIDeselect",					FGROUP_EXIDeselect,			0 },
	{  0x170,   30,     7,     5,     8,     9,	EXIProbe,				sizeof(EXIProbe),				"__EXIProbe",					FGROUP___EXIProbe,			0 },
	{  0x170,   30,     7,     5,     8,    10,	EXIProbe,				sizeof(EXIProbe),				"__EXIProbe",					FGROUP___EXIProbe,			0 },
	{  0x164,   30,     4,     5,     8,    10,	EXIProbe,				sizeof(EXIProbe),				"__EXIProbe",					FGROUP___EXIProbe,			0 },
//	{  0x378,   69,    11,    26,    20,    20,	EXIGetID,				sizeof(EXIGetID),				"EXIGetID",						FGROUP_NONE,				0 },
	{   0xEC,   24,     6,     6,     3,     7,	__CARDReadStatus,		sizeof(__CARDReadStatus),		"__CARDReadStatus",				FGROUP_NONE,				0 },
	{   0xA8,   17,     5,     4,     3,     5,	__CARDReadStatus+8,		8,								"__CARDClearStatus",			FGROUP_NONE,				0 },
	{  0x1B0,   32,     6,     8,    13,    14,	(u8*)NULL,				FCODE___CARDStat_A,				"__CARDStat A",					FGROUP_NONE,				0 },
	{  0x19C,   38,     8,     6,    13,    14,	(u8*)NULL,				FCODE___CARDStat_B,				"__CARDStat B",					FGROUP_NONE,				0 },
//	{  0x130,   33,     8,     6,     5,     2,	__CARDReadSegment,		sizeof(__CARDReadSegment),		"__CARDReadSegment",			FGROUP_NONE,				0 },
//	{   0x60,    7,     6,     1,     1,     3,	__CARDRead,				sizeof(__CARDRead),				"__CARDRead",					FGROUP_NONE,				0 },
//	{   0xDC,   17,     9,     4,     3,     2,	__CARDEraseSector,		sizeof(__CARDEraseSector),		"__CARDEraseSector",			FGROUP_NONE,				0 },
};
