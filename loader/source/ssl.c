/* Code taken from http://wiibrew.org/wiki//dev/net/ssl/code */

#include <ogc/machine/processor.h> //needed for patches -FIX94
#include <gccore.h>
#include <string.h>
#include "ssl.h"

#define ISALIGNED(x) ((((u32)x)&0x1F)==0)

static char __ssl_fs[] ATTRIBUTE_ALIGN(32) = "/dev/net/ssl";

static s32 __ssl_fd = -1;
static s32 __ssl_hid = -1;

u32 ssl_init(void)
{
	if(__ssl_hid < 0 ) {
		__ssl_hid = iosCreateHeap(SSL_HEAP_SIZE);
		if(__ssl_hid < 0){
			return __ssl_hid;
		}
		//some very dirty ssl patches for ios58 only -FIX94
		write16(0xD8B420A, 0);

		//ssl error -9 patch (wrong host)
		DCInvalidateRange( (void*)0x93CC1AC0, 0x20 );
		write32(0x93CC1AC0, 0xE328F102); //set "negative" flag
		DCFlushRange( (void*)0x93CC1AC0, 0x20 );

		//ssl error -10 patch (wrong root cert)
		DCInvalidateRange( (void*)0x93CC1B40, 0x20 );
		write32(0x93CC1B50, 0xEA000009); //beq->b
		DCFlushRange( (void*)0x93CC1B40, 0x20 );

		DCInvalidateRange( (void*)0x93CC1B80, 0x20 );
		write32(0x93CC1B94, 0xEA000008); //bne->b
		DCFlushRange( (void*)0x93CC1B80, 0x20 );

		//ssl error -11 patch (wrong client cert?)
		DCInvalidateRange( (void*)0x93CC1BE0, 0x20 );
		write32(0x93CC1BF8, 0xEA000016); //ble->b
		DCFlushRange( (void*)0x93CC1BE0, 0x20 );
	}

	return 0;
}

u32 ssl_open(void)
{
	s32 ret;

	if (__ssl_fd < 0) {
		ret = IOS_Open(__ssl_fs,0);
		if(ret<0){
			return ret;
		}
		__ssl_fd = ret;
	}

	return 0;
}

u32 ssl_close(void)
{
	s32 ret;

	if(__ssl_fd < 0){
		return 0;
	}

	ret = IOS_Close(__ssl_fd);

	__ssl_fd = -1;

	if(ret<0){
		return ret;
	}

	return 0;
}

s32 ssl_new(u8 * CN, u32 ssl_verify_options)
{
	s32 ret;
	s32 aContext[8] ATTRIBUTE_ALIGN(32);
	u32 aVerify_options[8] ATTRIBUTE_ALIGN(32);

	ret = ssl_open();
	if(ret){
		return ret;
	}

	aVerify_options[0] = ssl_verify_options;

	if(ISALIGNED(CN)){ //Avoid alignment if the input is aligned
		ret = IOS_IoctlvFormat(__ssl_hid, __ssl_fd, IOCTLV_SSL_NEW, "d:dd", aContext, 0x20, aVerify_options, 0x20, CN, 0x100);
	}else{
		u8 *aCN = NULL;

		aCN = iosAlloc(__ssl_hid, 0x100);
		if (!aCN) {
			return IPC_ENOMEM;
		}

		memcpy(aCN, CN, 0x100);
		ret = IOS_IoctlvFormat(__ssl_hid, __ssl_fd, IOCTLV_SSL_NEW, "d:dd", aContext, 0x20, aVerify_options, 0x20, aCN, 0x100);

		if(aCN){
			iosFree(__ssl_hid, aCN);
		}
	}

	ssl_close();

	return (ret ? ret : aContext[0]);
}

s32 ssl_setbuiltinclientcert(s32 ssl_context, s32 index)
{
	s32 aSsl_context[8] ATTRIBUTE_ALIGN(32);
	s32 aIndex[8] ATTRIBUTE_ALIGN(32);
	s32 aResponse[8] ATTRIBUTE_ALIGN(32);
	s32 ret;

	ret = ssl_open();
	if(ret){
		return ret;
	}

	aSsl_context[0] = ssl_context;
	aIndex[0] = index;
	ret = IOS_IoctlvFormat(__ssl_hid, __ssl_fd, IOCTLV_SSL_SETBUILTINCLIENTCERT, "d:dd", aResponse, 32, aSsl_context, 32, aIndex, 32);
	ssl_close();

	return (ret ? ret : aResponse[0]);
}

s32 ssl_setrootca(s32 ssl_context, const void *root, u32 length)
{
	s32 aSsl_context[8] ATTRIBUTE_ALIGN(32);
	s32 aResponse[8] ATTRIBUTE_ALIGN(32);
	s32 ret;

	ret = ssl_open();
	if(ret){
		return ret;
	}

	aSsl_context[0] = ssl_context;

	if(ISALIGNED(root)){ //Avoid alignment if the input is aligned
		ret = IOS_IoctlvFormat(__ssl_hid, __ssl_fd, IOCTLV_SSL_SETROOTCA, "d:dd", aResponse, 0x20, aSsl_context, 0x20, root, length);
	}else{
		u8 *aRoot = NULL;

		aRoot = iosAlloc(__ssl_hid, length);
		if (!aRoot) {
			return IPC_ENOMEM;
		}

		memcpy(aRoot, root, length);
		ret = IOS_IoctlvFormat(__ssl_hid, __ssl_fd, IOCTLV_SSL_SETROOTCA, "d:dd", aResponse, 0x20, aSsl_context, 0x20, aRoot, length);

		if(aRoot){
			iosFree(__ssl_hid, aRoot);
		}
	}

	ssl_close();

	return (ret ? ret : aResponse[0]);
}

s32 ssl_connect(s32 ssl_context, s32 socket)
{
	s32 aSsl_context[8] ATTRIBUTE_ALIGN(32);
	s32 aSocket[8] ATTRIBUTE_ALIGN(32);
	s32 aResponse[8] ATTRIBUTE_ALIGN(32);
	s32 ret;

	ret = ssl_open();
	if(ret){
		return ret;
	}

	aSsl_context[0] = ssl_context;
	aSocket[0] = socket;
	ret = IOS_IoctlvFormat(__ssl_hid, __ssl_fd, IOCTLV_SSL_CONNECT, "d:dd", aResponse, 0x20, aSsl_context, 0x20, aSocket, 0x20);
	ssl_close();

	return (ret ? ret : aResponse[0]);
}

s32 ssl_handshake(s32 ssl_context)
{

	s32 aSsl_context[8] ATTRIBUTE_ALIGN(32);
	s32 aResponse[8] ATTRIBUTE_ALIGN(32);
	s32 ret;

	ret = ssl_open();
	if(ret){
		return ret;
	}

	aSsl_context[0] = ssl_context;
	ret = IOS_IoctlvFormat(__ssl_hid, __ssl_fd, IOCTLV_SSL_HANDSHAKE, "d:d", aResponse, 0x20, aSsl_context, 0x20);
	ssl_close();

	return (ret ? ret : aResponse[0]);
}

s32 ssl_read(s32 ssl_context, void* buffer, u32 length)
{
	s32 aSsl_context[8] ATTRIBUTE_ALIGN(32);
	s32 aResponse[8] ATTRIBUTE_ALIGN(32);
	s32 ret;

	ret = ssl_open();
	if(ret){
		return ret;
	}

	if(!buffer){
		return IPC_EINVAL;
	}

	u8 *aBuffer = NULL;
	aBuffer = iosAlloc(__ssl_hid, length);
	if (!aBuffer) {
		return IPC_ENOMEM;
	}

	aSsl_context[0] = ssl_context;
	ret = IOS_IoctlvFormat(__ssl_hid, __ssl_fd, IOCTLV_SSL_READ, "dd:d", aResponse, 0x20, aBuffer, length, aSsl_context, 0x20);
	ssl_close();

	if(ret == IPC_OK){
		memcpy(buffer, aBuffer, aResponse[0]);
	}

	if(aBuffer){
		iosFree(__ssl_hid, aBuffer);
	}

	return (ret ? ret : aResponse[0]);
}

s32 ssl_write(s32 ssl_context, const void *buffer, u32 length)
{
	s32 aSsl_context[8] ATTRIBUTE_ALIGN(32);
	s32 aResponse[8] ATTRIBUTE_ALIGN(32);
	s32 ret;

	ret = ssl_open();
	if(ret){
		return ret;
	}

	if(!buffer){
		return IPC_EINVAL;
	}

	aSsl_context[0] = ssl_context;

	if(ISALIGNED(buffer)){ //Avoid alignment if the input is aligned
		ret = IOS_IoctlvFormat(__ssl_hid, __ssl_fd, IOCTLV_SSL_WRITE, "d:dd", aResponse, 0x20, aSsl_context, 0x20, buffer, length);
	}else{
		u8 *aBuffer = NULL;
		aBuffer = iosAlloc(__ssl_hid, length);
		if (!aBuffer) {
			return IPC_ENOMEM;
		}

		memcpy(aBuffer, buffer, length);
		ret = IOS_IoctlvFormat(__ssl_hid, __ssl_fd, IOCTLV_SSL_WRITE, "d:dd", aResponse, 0x20, aSsl_context, 0x20, aBuffer, length);
	}

	ssl_close();

	return (ret ? ret : aResponse[0]);
}

s32 ssl_shutdown(s32 ssl_context)
{
	s32 aSsl_context[8] ATTRIBUTE_ALIGN(32);
	s32 aResponse[8] ATTRIBUTE_ALIGN(32);
	s32 ret;

	ret = ssl_open();
	if(ret){
		return ret;
	}

	aSsl_context[0] = ssl_context;

	ret = IOS_IoctlvFormat(__ssl_hid, __ssl_fd, IOCTLV_SSL_SHUTDOWN, "d:d", aResponse, 0x20, aSsl_context, 0x20);

	ssl_close();

	return (ret ? ret : aResponse[0]);
}
