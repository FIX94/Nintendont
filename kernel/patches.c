/*

Nintendont (Kernel) - Playing Gamecubes in Wii mode on a Wii U

Copyright (C) 2013  crediar
Copyright (C) 2014 - 2016 FIX94

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
	FPATCH_SetInterruptMask = 1<<0,
	FPATCH_OSDispatchIntr = 1<<1,
	FPATCH_DVDIntrHandler = 1<<2,
	FPATCH_VIConfigure = 1<<3,
	FPATCH_GXInit = 1<<4,
	FPATCH_CARDUnlock = 1<<5,
	FPATCH_OSSleepThread = 1<<6,
	FPATCH_VideoModes = 1<<7,
	FPATCH_DSP_ROM = 1<<8,
	FPATCH_GXBegin = 1<<9,
	FPATCH_GXDrawDone = 1<<10,
	FPATCH_getTiming = 1<<11,
};

enum
{
	FCODES								= 0xdead0000,
	FCODE_ARInit,
	FCODE_ARStartDMA,
	FCODE_AIResetStreamCount,
	FCODE_Return,
										//__AI_set_stream_sample_rate
	FCODE_GXInitTlutObj,
										//GXInitTlutObj_A,
										//GXInitTlutObj_B,
	FCODE_GXLoadTlut,
	FCODE__SITransfer, 
										//_SITransfer_A,
										//_SITransfer_B,
	FCODE_CompleteTransfer,
										//CompleteTransfer_A,
										//CompleteTransfer_B,
										//CompleteTransfer_C,
	FCODE_SIInit,
										//SIInit_A,
										//SIInit_B,
										//SIInit_C,
	FCODE_SIPollingInterrupt,
										//SIEnablePollingInterrupt_A,
										//SIEnablePollingInterrupt_B,
	FCODE___ARHandler,
	FCODE___ARChecksize_A,
	FCODE___ARChecksize_B,
	FCODE___ARChecksize_C,
	FCODE___ARChecksize_DBG_A,
	FCODE___ARChecksize_DBG_B,
	FCODE___OSReadROM,
	FCODE_C_MTXPerspective,
	FCODE_C_MTXLightPerspective,
	FCODE_J3DUClipper_clip,
	//FCODE_C_MTXOrtho,
	FCODE_DVDSendCMDEncrypted,
	FCODE_IsTriforceType3,
	FCODE_GCAMSendCommand,
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
	FCODE_PADRead,
	FCODE___PADSetSamplingRate,
	FCODE_PADControlAllMotors,
	FCODE_PADControlMotor,
	FCODE_PADIsBarrel,
	FCODE_EXIDMA,
	FCODE_EXIUnlock,
	FCODE___CARDStat_A,
	FCODE___CARDStat_B,
	FCODE___CARDStat_C,
	FCODE_ReadROM,
	FCODE___OSResetHandler,
	FCODE_OSGetResetState,
	FCODE___OSInitAudioSystem_A,
	FCODE___OSInitAudioSystem_B,
	FCODE___DSPHandler,
	FCODE_PatchPatchBuffer,
	FCODE_PrsLoad,
	FCODE_DolEntryMod,
	FCODE_AppLoad,
} FPatternCodes;

enum
{
	FGROUP_NONE				= 0x0,
	FGROUP___GXSetVAT,
	FGROUP_GXInitTlutObj,
	FGROUP_GXLoadTlut,
	FGROUP_ARInit,
	FGROUP_ARStartDMA,
	FGROUP_SIGetType,
	FGROUP__SITransfer,
	FGROUP_CompleteTransfer,
	FGROUP_SIInit,
	FGROUP_SIPollingInterrupt,
	FGROUP___ARChecksize,
	FGROUP_DVDLowRead,
	FGROUP_DVDLowAudioStream,
	FGROUP___fwrite,
	FGROUP_PADRead,
	FGROUP_PADControlAllMotors,
	FGROUP_PADControlMotor,
	FGROUP_TCIntrruptHandler,
	FGROUP_EXIntrruptHandler,
	FGROUP_EXILock,
	FGROUP_EXIUnlock,
	FGROUP_EXISelect,
	FGROUP_EXIImm,
	FGROUP_EXIDMA,
	FGROUP_EXISync,
	FGROUP_EXIDeselect,
	FGROUP___EXIProbe,
	FGROUP_CARDStat,
	FGROUP_OSGetResetState,
	FGROUP___OSInitAudioSystem,
	FGROUP_ReadROM,
} FPatternGroups;

FuncPattern NormalFPatterns[] =
{
//	{   0xA8,   10,    4,    4,    6,    3,	DVDGetDriveStatus,	sizeof(DVDGetDriveStatus),	"DVDGetDriveStatus",	NULL,		FGROUP_NONE,				0 },
#ifndef AUDIOSTREAM
	{   0xD4,   13,    8,   11,    2,    7,	NULL,				FCODE_AIResetStreamCount,"AIResetStreamSampleCount",NULL,		FGROUP_NONE,				0 },
#endif
	{   0x98,    8,    3,    0,    3,    5,	NULL,				FCODE___GXSetVAT,			"__GXSetVAT",			"A",		FGROUP___GXSetVAT,			0 },
	{   0x84,    7,    3,    0,    1,    3,	NULL,				FCODE___GXSetVAT,			"__GXSetVAT",			"B",		FGROUP___GXSetVAT,			0 },

	{   0x44,    4,    4,    0,    0,    2,	NULL,				FCODE_GXInitTlutObj,		"GXInitTlutObj",		"A",		FGROUP_GXInitTlutObj,		0 },
	{   0x34,    5,    4,    0,    0,    0,	NULL,				FCODE_GXInitTlutObj,		"GXInitTlutObj",		"B",		FGROUP_GXInitTlutObj,		0 },
	{  0x13C,   28,    6,    6,    5,    4,	NULL,				FCODE_GXInitTlutObj,		"GXInitTlutObj",		"DBG",		FGROUP_GXInitTlutObj,		0 },

	{   0x98,   16,   10,    2,    0,    4,	NULL,				FCODE_GXLoadTlut,			"GXLoadTlut",			"A",		FGROUP_GXLoadTlut,			0 },
	{   0x94,   16,   10,    2,    0,    4,	NULL,				FCODE_GXLoadTlut,			"GXLoadTlut",			"B",		FGROUP_GXLoadTlut,			0 },

	{   0xEC,    9,    6,    2,    0,    8,	NULL,				FCODE_ARStartDMA,			"ARStartDMA",			NULL,		FGROUP_ARStartDMA,			0 },
	{  0x16C,   29,    3,    5,    3,    9,	NULL,				FCODE_ARStartDMA,			"ARStartDMA",			"DBG",		FGROUP_ARStartDMA,			0 },

	{   0xB8,   17,   10,    5,    1,    2,	NULL,				FCODE_ARInit,				"ARInit",				"A",		FGROUP_ARInit,				0 },
	{   0xC0,   18,   10,    6,    1,    2,	NULL,				FCODE_ARInit,				"ARInit",				"B",		FGROUP_ARInit,				0 },
	{   0xF0,   21,   12,    5,    1,    2,	NULL,				FCODE_ARInit,				"ARInit",				"C",		FGROUP_ARInit,				0 },
	{  0x168,   31,   13,   10,    2,    3,	NULL,				FCODE_ARInit,				"ARInit",				"DBG A",	FGROUP_ARInit,				0 },
	{  0x100,   23,   11,    6,    2,    3,	NULL,				FCODE_ARInit,				"ARInit",				"DBG B",	FGROUP_ARInit,				0 },

	{  0x910,   87,   33,   18,    5,   63,	NULL,				FCODE___ARChecksize_A,		"__ARChecksize",		"A",		FGROUP___ARChecksize,		0 },
	{ 0x17F0,  204,   51,   27,    5,  178,	NULL,				FCODE___ARChecksize_B,		"__ARChecksize",		"B",		FGROUP___ARChecksize,		0 },
	{  0xEC8,  129,   29,   32,    9,   80,	NULL,				FCODE___ARChecksize_C,		"__ARChecksize",		"C",		FGROUP___ARChecksize,		0 },
	{  0x530,  183,    4,   58,   13,   16,	NULL,				FCODE___ARChecksize_DBG_A,	"__ARChecksize",		"DBG A",	FGROUP___ARChecksize,		0 },
	{  0x304,   97,   11,   31,    7,   10,	NULL,				FCODE___ARChecksize_DBG_B,	"__ARChecksize",		"DBG B",	FGROUP___ARChecksize,		0 },

	{   0x74,   10,    3,    4,    1,    3,	NULL,				FCODE___ARHandler,			"__ARHandler",			NULL,		FGROUP_NONE,				0 },

	{  0x158,   26,   22,    5,   13,    2,	ARQPostRequest,		ARQPostRequest_size,		"ARQPostRequest",		NULL,		FGROUP_NONE,				0 },

	{   0xE8,   14,    7,    6,   10,    6,	NULL,				FCODE___PADSetSamplingRate,	"__PADSetSamplingRate",	NULL,		FGROUP_NONE,				0 },
#ifdef PATCHALL
	{   0xCC,    3,    3,    1,    0,    3,	NULL,				FCODE_C_MTXPerspective,		"C_MTXPerspective",		NULL,		FGROUP_NONE,				0 },
	{   0xC8,    3,    3,    1,    0,    3,	NULL,				FCODE_C_MTXLightPerspective,"C_MTXLightPerspective",NULL,		FGROUP_NONE,				0 },
//	{  0x144,    9,    3,    1,   10,    6,	NULL,				FCODE_J3DUClipper_clip,		"J3DUClipper::clip()",	NULL,		FGROUP_NONE,				0 },	// These are two different functions
//	{  0x2E4,   39,    8,    3,   13,    9,	NULL,				FCODE_J3DUClipper_clip,		"J3DUClipper::clip()",	NULL,		FGROUP_NONE,				0 },
//	{   0x94,    0,    0,    0,    0,    0,	NULL,				FCODE_J3DUClipper_clip,		"C_MTXOrtho",						FGROUP_NONE,				0 },
#endif
	{  0x308,   40,   18,   10,   23,   17,	NULL,				FCODE___fwrite,				"__fwrite",				"A",		FGROUP___fwrite,			0 },
	{  0x338,   48,   20,   10,   24,   16,	NULL,				FCODE___fwrite,				"__fwrite",				"B",		FGROUP___fwrite,			0 },
	{  0x2D8,   41,   17,    8,   21,   13,	NULL,				FCODE___fwrite,				"__fwrite",				"C",		FGROUP___fwrite,			0 },
//	{  0x1FC,   47,    4,   14,   18,    7,	NULL,				FCODE___fwrite_D,			"__fwrite D",						FGROUP___fwrite,			0 },

#ifdef PATCHALL
	{   0xF0,   20,   11,    3,    3,    9,	NULL,				FCODE___OSResetHandler,	"__OSResetSWInterruptHandler",NULL,		FGROUP_NONE,				0 },

	{  0x294,   39,   16,    5,   16,   46,	NULL,				FCODE_OSGetResetState,		"OSGetResetButtonState","A",		FGROUP_OSGetResetState,		0 },
	{  0x2A0,   40,   16,    5,   17,   46,	NULL,				FCODE_OSGetResetState,		"OSGetResetButtonState","B",		FGROUP_OSGetResetState,		0 },
	{  0x1F0,   34,   14,    6,   12,   28,	NULL,				FCODE_OSGetResetState,		"OSGetResetButtonState","C",		FGROUP_OSGetResetState,		0 },
#endif
	{  0x28C,   70,    8,    8,   10,    4,	NULL,				FCODE___OSInitAudioSystem_A,"__OSInitAudioSystem",	"DBG A",	FGROUP___OSInitAudioSystem,	0 },
	{  0x2B8,   77,    8,   12,   10,    4,	NULL,				FCODE___OSInitAudioSystem_B,"__OSInitAudioSystem",	"DBG B",	FGROUP___OSInitAudioSystem,	0 },
#ifdef AUDIOSTREAM
	{  0x420,  103,   23,   34,   32,    9,	NULL,				FCODE___DSPHandler,			"__DSPHandler",			NULL,		FGROUP_NONE,				0 },
#else
	{   0x94,   18,   10,    2,    0,    2,	DVDLowReadAudioNULL,sizeof(DVDLowReadAudioNULL),"DVDLowAudioStream",	"A",		FGROUP_DVDLowAudioStream,	0 },
	{   0x8C,   16,   12,    1,    0,    2,	DVDLowReadAudioNULL,sizeof(DVDLowReadAudioNULL),"DVDLowAudioStream",	"B",		FGROUP_DVDLowAudioStream,	0 },
	{   0x88,   18,    8,    2,    0,    2,	DVDLowAudioStatusNULL,	sizeof(DVDLowAudioStatusNULL),"DVDLowRequestAudioStatus",	NULL,FGROUP_NONE,			0 },
	{   0x98,   19,    8,    2,    1,    3,	DVDLowAudioConfigNULL,	sizeof(DVDLowAudioConfigNULL),"DVDLowAudioBufferConfig",	NULL,FGROUP_NONE,			0 },
#endif
};

FuncPattern TRIFPatterns[] =
{
#ifdef TRI_DI_PATCH
	{   0xB4,   18,   11,    1,    0,    7,	NULL,				FCODE_DVDSendCMDEncrypted,	"DVDSendCMDEncrypted",	NULL,		FGROUP_NONE,				0 },
#endif
	{  0x484,  104,   30,   20,   17,   21, NULL,				FCODE_IsTriforceType3,		"IsTriforceType3",		NULL,		FGROUP_NONE,				0 },
	{   0x54,   10,    2,    2,    0,    2,	NULL,				FCODE_GCAMSendCommand,		"GCAMSendCommand",		NULL,		FGROUP_NONE,				0 },
	{  0x168,   22,   10,    7,    6,   10,	SITransfer,			sizeof(SITransfer),			"SITransfer",			NULL,		FGROUP_NONE,				0 },
};

FuncPattern SIFPatterns[] =
{
	{  0x2F8,   60,   22,    2,   16,   25,	NULL,				FCODE_CompleteTransfer,		"CompleteTransfer",		"A",		FGROUP_CompleteTransfer,	0 },
	{  0x240,   40,   14,    0,   13,   11,	NULL,				FCODE_CompleteTransfer,		"CompleteTransfer",		"B",		FGROUP_CompleteTransfer,	0 },
	{  0x180,   29,    9,    3,    9,    9,	NULL,				FCODE_CompleteTransfer,		"CompleteTransfer",		"C",		FGROUP_CompleteTransfer,	0 },
	{   0xE0,   18,    4,    0,    6,    3,	NULL,				FCODE_CompleteTransfer,		"CompleteTransfer",		"DBG",		FGROUP_CompleteTransfer,	0 },

	{   0x94,    8,   10,    2,    4,    2,	NULL,				FCODE_SIPollingInterrupt,	"SIEnablePollingInterrupt","A",		FGROUP_SIPollingInterrupt,	0 },
	{   0xA4,    9,    5,    2,    6,    4,	NULL,				FCODE_SIPollingInterrupt,	"SIEnablePollingInterrupt","B",		FGROUP_SIPollingInterrupt,	0 },

	{   0xB0,   21,    9,    8,    0,    2,	NULL,				FCODE_SIInit,				"SIInit",				"A",		FGROUP_SIInit,				0 },
	{   0x70,   13,    8,    2,    0,    2,	NULL,				FCODE_SIInit,				"SIInit",				"B",		FGROUP_SIInit,				0 },
	{   0x90,   17,    8,    6,    0,    2,	NULL,				FCODE_SIInit,				"SIInit",				"C",		FGROUP_SIInit,				0 },
	{   0xA0,   20,    8,    7,    0,    2,	NULL,				FCODE_SIInit,				"SIInit",				"D",		FGROUP_SIInit,				0 },
	{   0xB0,   22,    9,    8,    0,    2,	NULL,				FCODE_SIInit,				"SIInit",				"E",		FGROUP_SIInit,				0 },
	{   0x9C,   19,    9,    6,    0,    2,	NULL,				FCODE_SIInit,				"SIInit",				"F",		FGROUP_SIInit,				0 },
	{   0xA8,   21,    9,    7,    0,    2,	NULL,				FCODE_SIInit,				"SIInit",				"DBG A",	FGROUP_SIInit,				0 },
	{   0x7C,   15,    9,    2,    0,    2,	NULL,				FCODE_SIInit,				"SIInit",				"DBG B",	FGROUP_SIInit,				0 },

	{  0x208,   38,   18,    3,   13,   10,	NULL,				FCODE__SITransfer,			"__SITransfer",			"A",		FGROUP__SITransfer,			0 },
	{  0x204,   37,   18,    3,   13,   11,	NULL,				FCODE__SITransfer,			"__SITransfer",			"B",		FGROUP__SITransfer,			0 },
	{  0x208,   38,   11,    7,   13,    9,	NULL,				FCODE__SITransfer,			"__SITransfer",			"C",		FGROUP__SITransfer,			0 },
	{  0x204,   37,   11,    7,   13,    9,	NULL,				FCODE__SITransfer,			"__SITransfer",			"DBG",		FGROUP__SITransfer,			0 },

	{  0x1C0,   35,    9,    8,    7,   19,	SIGetType,			SIGetType_size,				"SIGetType",			"A",		FGROUP_SIGetType,			0 },
	{  0x1F4,   27,    9,    9,    9,   24,	SIGetType,			SIGetType_size,				"SIGetType",			"B",		FGROUP_SIGetType,			0 },

	{  0x3A8,   86,   13,   27,   17,   24,	NULL,				FCODE_PADRead,				"PADRead",				"A",		FGROUP_PADRead,				0 },
	{  0x2FC,   73,    8,   23,   16,   15,	NULL,				FCODE_PADRead,				"PADRead",				"B",		FGROUP_PADRead,				0 },
	{  0x3B0,   87,   13,   27,   17,   25,	NULL,				FCODE_PADRead,				"PADRead",				"C",		FGROUP_PADRead,				0 },
	{  0x334,   78,    7,   20,   17,   19,	NULL,				FCODE_PADRead,				"PADRead",				"D",		FGROUP_PADRead,				0 },
	{  0x1FC,   49,    4,   10,   11,   11,	NULL,				FCODE_PADRead,				"PADRead",				"IPL",		FGROUP_PADRead,				0 },
	{  0x2A8,   66,    4,   20,   17,   14,	NULL,				FCODE_PADRead,				"PADRead",				"DBG A",	FGROUP_PADRead,				0 },
	{  0x2AC,   65,    3,   15,   16,   18,	NULL,				FCODE_PADRead,				"PADRead",				"DBG B",	FGROUP_PADRead,				0 },

	{   0xB4,    8,    2,    5,    4,    5,	NULL,				FCODE_PADControlAllMotors,	"PADControlAllMotors",	"A",		FGROUP_PADControlAllMotors,	0 },
	{   0xC8,    9,    2,    5,    5,    5,	NULL,				FCODE_PADControlAllMotors,	"PADControlAllMotors",	"B",		FGROUP_PADControlAllMotors,	0 },
	{   0xC8,   13,    2,    4,    4,    6,	NULL,				FCODE_PADControlAllMotors,	"PADControlAllMotors",	"C",		FGROUP_PADControlAllMotors,	0 },

	{   0xB4,   11,    5,    5,    3,    5,	NULL,				FCODE_PADControlMotor,		"PADControlMotor",		"A",		FGROUP_PADControlMotor,		0 },
	{   0xA0,   10,    5,    5,    2,    5,	NULL,				FCODE_PADControlMotor,		"PADControlMotor",		"B",		FGROUP_PADControlMotor,		0 },
	{   0xB8,   14,    5,    4,    2,    7,	NULL,				FCODE_PADControlMotor,		"PADControlMotor",		"C",		FGROUP_PADControlMotor,		0 },
	{   0xB0,   10,    2,    6,    3,    6,	NULL,				FCODE_PADControlMotor,		"PADControlMotor",		"DBG A",	FGROUP_PADControlMotor,		0 },
	{   0xC8,   14,    2,    5,    3,    8,	NULL,				FCODE_PADControlMotor,		"PADControlMotor",		"DBG B",	FGROUP_PADControlMotor,		0 },

	{   0x14,    1,    0,    0,    2,    0,	NULL,				FCODE_PADIsBarrel,			"PADIsBarrel",			NULL,		FGROUP_NONE,				0 },
};

FuncPattern EXIFPatterns[] =
{
	{  0x258,   36,    8,    5,   12,   32,	EXIImm,				EXIImm_size,				"EXIImm",				"A",		FGROUP_EXIImm,				0 },
	{  0x258,   27,    8,    5,   12,   17,	EXIImm,				EXIImm_size,				"EXIImm",				"B",		FGROUP_EXIImm,				0 },
	{  0x158,   24,    7,    5,    7,    9,	EXIImm,				EXIImm_size,				"EXIImm",				"PKM",		FGROUP_EXIImm,				0 },
	{  0x1E4,   38,    7,    9,   12,    9,	EXIImm,				EXIImm_size,				"EXIImm",				"DBG",		FGROUP_EXIImm,				0 },

	{   0xE8,   17,    7,    5,    2,    5,	NULL,				FCODE_EXIDMA,				"EXIDMA",				"A",		FGROUP_EXIDMA,				0 },
	{   0xE8,   17,    7,    5,    2,    4,	NULL,				FCODE_EXIDMA,				"EXIDMA",				"B",		FGROUP_EXIDMA,				0 },
	{  0x124,   28,    8,    5,    2,    8,	NULL,				FCODE_EXIDMA,				"EXIDMA",				"PKM",		FGROUP_EXIDMA,				0 },
	{  0x1C0,   42,    5,   10,    8,    6,	NULL,				FCODE_EXIDMA,				"EXIDMA",				"DBG",		FGROUP_EXIDMA,				0 },

	{  0x234,   39,    3,    3,   12,   19,	EXILock,			EXILock_size,				"EXISync",				"A",		FGROUP_EXISync,				0 },
	{  0x248,   40,    3,    4,   13,   19,	EXILock,			EXILock_size,				"EXISync",				"B",		FGROUP_EXISync,				0 },
	{  0x204,   31,    3,    3,   11,   17,	EXILock,			EXILock_size,				"EXISync",				"C",		FGROUP_EXISync,				0 },
	{  0x234,   35,    3,    3,   12,   17,	EXILock,			EXILock_size,				"EXISync",				"D",		FGROUP_EXISync,				0 },
	{  0x1CC,   27,    6,    2,   10,   17,	EXILock,			EXILock_size,				"EXISync",				"IPL",		FGROUP_EXISync,				0 },
	{  0x16C,   26,    3,    3,    9,    7,	EXILock,			EXILock_size,				"EXISync",				"PKM",		FGROUP_EXISync,				0 },
	{  0x1A8,   34,    2,    7,    9,    8,	EXILock,			EXILock_size,				"EXISync",				"DBG A",	FGROUP_EXISync,				0 },
	{  0x13C,   25,    2,    6,    7,    7,	EXILock,			EXILock_size,				"EXISync",				"DBG B",	FGROUP_EXISync,				0 },
	{  0x13C,   24,    2,    6,    7,    6,	EXILock,			EXILock_size,				"EXISync",				"DBG C",	FGROUP_EXISync,				0 },

	{  0x170,   30,    7,    5,    8,    9,	EXIProbe,			EXIProbe_size,				"__EXIProbe",			"A",		FGROUP___EXIProbe,			0 },
	{  0x170,   30,    7,    5,    8,   10,	EXIProbe,			EXIProbe_size,				"__EXIProbe",			"B",		FGROUP___EXIProbe,			0 },
	{  0x164,   30,    4,    5,    8,   10,	EXIProbe,			EXIProbe_size,				"__EXIProbe",			"C",		FGROUP___EXIProbe,			0 },
	{  0x1B0,   34,    6,    5,    8,    8,	EXIProbe,			EXIProbe_size,				"__EXIProbe",			"PKM",		FGROUP___EXIProbe,			0 },
	{  0x1B8,   38,    5,    7,   10,   10,	EXIProbe,			EXIProbe_size,				"__EXIProbe",			"DBG A",	FGROUP___EXIProbe,			0 },
	{  0x1BC,   39,    5,    7,   10,   10,	EXIProbe,			EXIProbe_size,				"__EXIProbe",			"DBG B",	FGROUP___EXIProbe,			0 },
	{  0x1AC,   38,    2,    7,   10,   10,	EXIProbe,			EXIProbe_size,				"__EXIProbe",			"DBG C",	FGROUP___EXIProbe,			0 },

	{  0x128,   18,    4,    6,   11,    8,	EXISelect,			EXISelect_size,				"EXISelect",			"A",		FGROUP_EXISelect,			0 },
	{  0x128,   18,    4,    6,   11,    7,	EXISelect,			EXISelect_size,				"EXISelect",			"B",		FGROUP_EXISelect,			0 },
	{  0x13C,   20,    4,    6,   11,    6,	EXISelect,			EXISelect_size,				"EXISelect",			"PKM",		FGROUP_EXISelect,			0 },
	{  0x1CC,   33,    3,   10,   17,    6,	EXISelect,			EXISelect_size,				"EXISelect",			"DBG",		FGROUP_EXISelect,			0 },

	{  0x10C,   20,    8,    6,   12,    4,	EXILock,			EXILock_size,				"EXIDeselect",			"A",		FGROUP_EXIDeselect,			0 },
	{  0x10C,   20,    8,    6,   12,    3,	EXILock,			EXILock_size,				"EXIDeselect",			"B",		FGROUP_EXIDeselect,			0 },
	{  0x10C,   20,    8,    6,   11,    3,	EXILock,			EXILock_size,				"EXIDeselect",			"IPL",		FGROUP_EXIDeselect,			0 },
	{  0x104,   17,    3,    6,   12,    4,	EXILock,			EXILock_size,				"EXIDeselect",			"PKM",		FGROUP_EXIDeselect,			0 },
	{  0x130,   22,    3,    7,   14,    5,	EXILock,			EXILock_size,				"EXIDeselect",			"DBG A",	FGROUP_EXIDeselect,			0 },
	{  0x12C,   21,    3,    7,   14,    5,	EXILock,			EXILock_size,				"EXIDeselect",			"DBG B",	FGROUP_EXIDeselect,			0 },

	{   0x7C,   10,    3,    0,    1,    7,	NULL,				FCODE_EXIIntrruptHandler,	"EXIntrruptHandler",	"A",		FGROUP_EXIntrruptHandler,	0 },
	{   0xC4,   19,    6,    4,    1,    7,	NULL,				FCODE_EXIIntrruptHandler,	"EXIntrruptHandler",	"B",		FGROUP_EXIntrruptHandler,	0 },
	{   0xC4,   19,    6,    4,    1,    8,	NULL,				FCODE_EXIIntrruptHandler,	"EXIntrruptHandler",	"C",		FGROUP_EXIntrruptHandler,	0 },
	{   0xBC,   16,    3,    4,    1,    3,	NULL,				FCODE_EXIIntrruptHandler,	"EXIntrruptHandler",	"PKM",		FGROUP_EXIntrruptHandler,	0 },
	{   0xC4,   20,    2,    6,    3,    3,	NULL,				FCODE_EXIIntrruptHandler,	"EXIntrruptHandler",	"DBG A",	FGROUP_EXIntrruptHandler,	0 },
	{   0xC8,   21,    2,    6,    3,    3,	NULL,				FCODE_EXIIntrruptHandler,	"EXIntrruptHandler",	"DBG B",	FGROUP_EXIntrruptHandler,	0 },
	{   0xA4,   16,    3,    2,    3,    2,	NULL,				FCODE_EXIIntrruptHandler,	"EXIntrruptHandler",	"DBG C",	FGROUP_EXIntrruptHandler,	0 },

	{  0x1F0,   34,    9,    1,    8,   21,	NULL,				FCODE_EXIIntrruptHandler,	"TCIntrruptHandler",	"A",		FGROUP_TCIntrruptHandler,	0 },
	{  0x214,   41,    9,    5,    8,   22,	NULL,				FCODE_EXIIntrruptHandler,	"TCIntrruptHandler",	"B",		FGROUP_TCIntrruptHandler,	0 },
	{  0x214,   37,    9,    5,    8,   21,	NULL,				FCODE_EXIIntrruptHandler,	"TCIntrruptHandler",	"C",		FGROUP_TCIntrruptHandler,	0 },
	{  0x158,   28,    5,    5,    6,    4,	NULL,				FCODE_EXIIntrruptHandler,	"TCIntrruptHandler",	"PKM",		FGROUP_TCIntrruptHandler,	0 },
	{   0xE4,   22,    3,    8,    3,    3,	NULL,				FCODE_EXIIntrruptHandler,	"TCIntrruptHandler",	"DBG A",	FGROUP_TCIntrruptHandler,	0 },
	{   0xE8,   23,    3,    8,    3,    3,	NULL,				FCODE_EXIIntrruptHandler,	"TCIntrruptHandler",	"DBG B",	FGROUP_TCIntrruptHandler,	0 },
	{   0xC4,   18,    4,    4,    3,    4,	NULL,				FCODE_EXIIntrruptHandler,	"TCIntrruptHandler",	"DBG C",	FGROUP_TCIntrruptHandler,	0 },
//	{   0xA8,   17,    6,    1,    1,    7,	NULL,				FCODE_EXIIntrruptHandler,	"TCIntrruptHandler E",			FGROUP_TCIntrruptHandler,	0 },

	{   0xF0,   17,    7,    5,    5,    7,	EXILock,			EXILock_size,				"EXILock",				"A",		FGROUP_EXILock,				0 },
	{   0xF0,   18,    7,    5,    5,    6,	EXILock,			EXILock_size,				"EXILock",				"B",		FGROUP_EXILock,				0 },
	{   0xF8,   19,    5,    5,    6,    6,	EXILock,			EXILock_size,				"EXILock",				"PKM",		FGROUP_EXILock,				0 },
	{  0x1A4,   35,    5,    9,   13,    6,	EXILock,			EXILock_size,				"EXILock",				"DBG",		FGROUP_EXILock,				0 },

	{   0xD8,   21,    8,    5,    3,    3,	NULL,				FCODE_EXIUnlock,			"EXIUnlock",			"A",		FGROUP_EXIUnlock,			0 },
	{   0xD8,   21,    8,    5,    3,    2,	NULL,				FCODE_EXIUnlock,			"EXIUnlock",			"B",		FGROUP_EXIUnlock,			0 },
	{   0xC4,   18,    4,    5,    3,    3,	NULL,				FCODE_EXIUnlock,			"EXIUnlock",			"PKM",		FGROUP_EXIUnlock,			0 },
	{   0xF0,   23,    4,    6,    5,    4,	NULL,				FCODE_EXIUnlock,			"EXIUnlock",			"DBG A",	FGROUP_EXIUnlock,			0 },
	{   0xEC,   22,    4,    6,    5,    4,	NULL,				FCODE_EXIUnlock,			"EXIUnlock",			"DBG B",	FGROUP_EXIUnlock,			0 },

//	{  0x378,   69,   11,   26,   20,   20,	EXIGetID,			sizeof(EXIGetID),			"EXIGetID",							FGROUP_NONE,				0 },
	{   0xEC,   24,    6,    6,    3,    7,	__CARDReadStatus,	__CARDReadStatus_size,		"__CARDReadStatus",		NULL,		FGROUP_NONE,				0 },
	{   0xA8,   17,    5,    4,    3,    5,	__CARDClearStatus,	__CARDClearStatus_size,		"__CARDClearStatus",	NULL,		FGROUP_NONE,				0 },
	{  0x1B0,   32,    6,    8,   13,   14,	NULL,				FCODE___CARDStat_A,			"__CARDStat",			"A",		FGROUP_CARDStat,			0 },
	{  0x19C,   38,    8,    6,   13,   14,	NULL,				FCODE___CARDStat_B,			"__CARDStat",			"B",		FGROUP_CARDStat,			0 },
	{  0x220,   37,    6,    9,   15,   25,	NULL,				FCODE___CARDStat_C,			"__CARDStat",			"C",		FGROUP_CARDStat,			0 },

	{   0x88,    9,    6,    1,    3,    2,	NULL,				FCODE_ReadROM,				"ReadROM",				"A",		FGROUP_ReadROM,				0 },
	{   0x80,    8,    6,    1,    4,    6,	NULL,				FCODE_ReadROM,				"ReadROM",				"B",		FGROUP_ReadROM,				0 },

	{  0x120,   28,    6,   10,    2,    7,	NULL,				FCODE___OSReadROM,			"__OSReadROM",			NULL,		FGROUP_NONE,				0 },
};

FuncPattern DatelFPatterns[] =
{
	{   0x48,    9,   2,    3,    0,    3,	NULL,				FCODE_DolEntryMod,			"DolEntryMod",			"Datel",	FGROUP_NONE,				0 },
	{   0xF8,    8,   6,    7,    1,    6,	NULL,				FCODE_DolEntryMod,			"DolEntryMod",			"DatelB",	FGROUP_NONE,				0 },
};

FuncPattern PSOFPatterns[] = 
{
	{  0x23C,   66,   24,   35,    0,    9,	NULL,				FCODE_PatchPatchBuffer,		"PatchBuffer",			"A",		FGROUP_NONE,			    0 },
	{  0x274,   51,   24,    7,   17,   16,	NULL,				FCODE_PatchPatchBuffer,		"PatchBuffer",			"B",		FGROUP_NONE,			    0 },
	{  0x1B0,   11,   10,   15,   11,    9,	NULL,				FCODE_PatchPatchBuffer,		"PatchBuffer",			"C",		FGROUP_NONE,			    0 },
	{   0x2C,    6,    2,    1,    0,    2,	NULL,				FCODE_PatchPatchBuffer,		"PatchBuffer",			"D",		FGROUP_NONE,			    0 },
	{  0x378,  110,   26,   52,    0,    9,	NULL,				FCODE_PatchPatchBuffer,		"PatchBuffer",			"E",		FGROUP_NONE,			    0 },
	{  0x378,  109,   26,   52,    0,   10,	NULL,				FCODE_PatchPatchBuffer,		"PatchBuffer",			"F",		FGROUP_NONE,			    0 },
	{  0x268,   52,    9,   39,    8,    7,	NULL,				FCODE_PrsLoad,				"PrsLoad",				NULL,		FGROUP_NONE,			    0 },
	{   0xC0,   22,    2,    7,    1,    4,	NULL,				FCODE_DolEntryMod,			"DolEntryMod",			NULL,		FGROUP_NONE,			    0 },
};

enum
{
	PCODE_NORMAL = 0,
	PCODE_TRI,
	PCODE_SI,
	PCODE_EXI,
	PCODE_DATEL,
	PCODE_PSO,
	PCODE_MAX,
} AllPGroups;

FuncPatterns AllFPatterns[] = 
{
	{ NormalFPatterns, sizeof(NormalFPatterns) / sizeof(FuncPattern), PCODE_NORMAL },
	{ TRIFPatterns, sizeof(TRIFPatterns) / sizeof(FuncPattern), PCODE_TRI },
	{ SIFPatterns, sizeof(SIFPatterns) / sizeof(FuncPattern), PCODE_SI },
	{ EXIFPatterns, sizeof(EXIFPatterns) / sizeof(FuncPattern), PCODE_EXI },
	{ DatelFPatterns, sizeof(DatelFPatterns) / sizeof(FuncPattern), PCODE_DATEL },
	{ PSOFPatterns, sizeof(PSOFPatterns) / sizeof(FuncPattern), PCODE_PSO },
};
