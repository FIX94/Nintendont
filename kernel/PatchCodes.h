#ifndef __PATCHCODES_H__
#define __PATCHCODES_H__

#include "global.h"

#include "asm/EXIImm.h"
#include "asm/EXISelect.h"
#include "asm/EXILock.h"
#include "asm/EXIDMA.h"
#include "asm/EXIProbe.h"
#include "asm/EXIGetID.h"
#include "asm/__CARDReadStatus.h"
#include "asm/__CARDClearStatus.h"
#include "asm/ReadROM.h"

#include "asm/ARQPostRequest.h"
#include "asm/ARStartDMA.h"
#include "asm/ARStartDMA_PM.h"
#include "asm/ARStartDMA_TC.h"
#include "asm/ARStartDMA_Hook.h"
#include "asm/__ARHandler.h"
#include "asm/SITransfer.h"
#include "asm/SIGetType.h"
#include "asm/FakeInterrupt.h"
#include "asm/FakeInterrupt_DBG.h"
#include "asm/FakeInterrupt_Datel.h"
#include "asm/__DVDInterruptHandler.h"
#include "asm/TCIntrruptHandler.h"
#include "asm/SIIntrruptHandler.h"
#include "asm/SIInitStore.h"
#include "asm/PADRead.h"
#include "asm/PADControlAllMotors.h"
#include "asm/PADControlMotor.h"
#include "asm/PADIsBarrel.h"
#include "asm/DVDSendCMDEncrypted.h"
#include "asm/GCAMSendCommand.h"
#include "asm/patch_fwrite_Log.h"
#include "asm/patch_fwrite_GC.h"
#include "asm/FakeRSWLoad.h"
#include "asm/FakeRSWStore.h"
#include "asm/FakeEntryLoad.h"
#include "asm/SwitcherPrs.h"
#include "asm/OSReportDM.h"
#include "asm/OSExceptionInit.h"
#include "asm/__DSPHandler.h"
#include "asm/__GXSetVAT.h"
#include "asm/GXInitTlutObj.h"
#include "asm/GXLoadTlut.h"
#include "asm/DatelTimer.h"
#include "asm/SonicRidersCopy.h"

#include "asm/MajoraAudioStream.h"
#include "asm/MajoraLoadRegs.h"
#include "asm/MajoraSaveRegs.h"

#include "asm/codehandler_stub.h"

#include "asm/SOInit.h"
#include "asm/SOStartup.h"
#include "asm/SOCleanup.h"
#include "asm/SOSocket.h"
#include "asm/SOClose.h"
#include "asm/SOListen.h"
#include "asm/SOAccept.h"
#include "asm/SOConnect.h"
#include "asm/SOBind.h"
#include "asm/SOShutdown.h"
#include "asm/SORecvFrom.h"
#include "asm/SOSendTo.h"
#include "asm/SOSetSockOpt.h"
#include "asm/SOFcntl.h"
#include "asm/SOPoll.h"
#include "asm/IPGetMacAddr.h"
#include "asm/IPGetNetmask.h"
#include "asm/IPGetAddr.h"
#include "asm/IPGetMtu.h"
#include "asm/IPGetLinkState.h"
#include "asm/IPGetConfigError.h"
#include "asm/IPSetConfigError.h"
#include "asm/IPClearConfigError.h"
#include "asm/HSPIntrruptHandler.h"

#include "asm/avetcp_init.h"
#include "asm/avetcp_term.h"
#include "asm/dns_set_server.h"
#include "asm/dns_clear_server.h"
#include "asm/dns_open_addr.h"
#include "asm/dns_get_addr.h"
#include "asm/dns_close.h"
#include "asm/DHCP_request_nb.h"
#include "asm/DHCP_get_gateway.h"
#include "asm/DHCP_get_dns.h"
#include "asm/DHCP_release.h"
#include "asm/tcp_create.h"
#include "asm/tcp_bind.h"
#include "asm/tcp_listen.h"
#include "asm/tcp_stat.h"
#include "asm/tcp_getaddr.h"
#include "asm/tcp_connect.h"
#include "asm/tcp_accept.h"
#include "asm/tcp_send.h"
#include "asm/tcp_receive.h"
#include "asm/tcp_abort.h"
#include "asm/tcp_delete.h"
#include "asm/Return0.h"
#include "asm/Return1.h"
#include "asm/ReturnMinus1.h"
#include "asm/KeyboardRead.h"

//this is the data from my wii disk drive
const u8 DiskDriveInfo[32] = {
	0x00,0x00,0x00,0x02,
	0x20,0x08,0x07,0x14, // 7/14/2008
	0x41,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00
};

static const u8 GXIntDfAt04[] = {
	0x02, 0x80, 0x01, 0xE0, 0x01, 0xE0, 0x00, 0x28, 0x00, 0x00, 0x02, 0x80, 0x01, 0xE0, 0x00, 0x00,
};

static const u8 GXDeflickerOn[] = {
	0x08, 0x08, 0x0A, 0x0C, 0x0A, 0x08, 0x08
};

static const u8 GXDeflickerOff[] = {
	0x00, 0x00, 0x15, 0x16, 0x15, 0x00, 0x00
};

static const u32 DVDGetDriveStatus[] = {
        0x38600000,     //  li		r3, 0
        0x4E800020      //  blr
};

#ifndef AUDIOSTREAM
// Audio streaming replacement functions copied from Swiss r92
static const u32 DVDLowAudioStatusNULL[17] = {
        // execute function(1); passed in on r4
        0x9421FFC0,     //  stwu        sp, -0x0040 (sp)
        0x7C0802A6,     //  mflr        r0
        0x90010000,     //  stw         r0, 0 (sp)
        0x7C8903A6,     //  mtctr       r4
        0x3C80C000,     //  lis         r4, 0xC000
        0x2E830000,     //  cmpwi       cr5, r3, 0
        0x4196000C,     //  beq-        cr5, +0xC ?
        0x38600001,     //  li          r3, 1
        0x48000008,     //  b           +0x8 ?
        0x38600000,     //  li          r3, 0
        0x90642F20,     //  stw         r3, 0x2F20 (r4)
        0x38600001,     //  li          r3, 1
        0x4E800421,     //  bctrl
        0x80010000,     //  lwz         r0, 0 (sp)
        0x7C0803A6,     //  mtlr        r0
        0x38210040,     //  addi        sp, sp, 64
        0x4E800020      //  blr
};

static const u32 DVDLowAudioConfigNULL[10] = {
        // execute callback(1); passed in on r5 without actually touching the drive!
        0x9421FFC0,     //  stwu        sp, -0x0040 (sp)
        0x7C0802A6,     //  mflr        r0
        0x90010000,     //  stw         r0, 0 (sp)
        0x7CA903A6,     //  mtctr       r5
        0x38600001,     //  li          r3, 1
        0x4E800421,     //  bctrl
        0x80010000,     //  lwz         r0, 0 (sp)
        0x7C0803A6,     //  mtlr        r0
        0x38210040,     //  addi        sp, sp, 64
        0x4E800020      //  blr
};

const u32 DVDLowReadAudioNULL[10] = {
        // execute callback(1); passed in on r6 without actually touching the drive!
        0x9421FFC0,     //  stwu        sp, -0x0040 (sp)
        0x7C0802A6,     //  mflr        r0
        0x90010000,     //  stw         r0, 0 (sp)
        0x7CC903A6,     //  mtctr       r6
        0x38600001,     //  li          r3, 1
        0x4E800421,     //  bctr;
        0x80010000,     //  lwz         r0, 0 (sp)
        0x7C0803A6,     //  mtlr        r0
        0x38210040,     //  addi        sp, sp, 64
        0x4E800020      //  blr
};
#endif

//function header is good enough to verify
static const u32 PADIsBarrelOri[] = {
		0x2C030000,		// cmpwi	r3,0
		0x4180000C,		// blt		0x10
		0x2C030004,		// cmpwi	r3,4
		0x4180000C,		// blt		0x18
		0x38600000,		// li		r3,0
		0x4E800020,		// blr
		0x3C008000		// lis		r0,0x8000
};

#endif
