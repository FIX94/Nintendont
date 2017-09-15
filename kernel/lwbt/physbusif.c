
/* Very similar to the LibOGC version, modified to work with nintendont */

#include "global.h"
#include "alloc.h"
#include "string.h"
#include "hci.h"
#include "btmemb.h"
#include "physbusif.h"

extern int dbgprintf( const char *fmt, ...);

#define STACKSIZE					32768

#define NUM_ACL_BUFS				30
#define NUM_CTRL_BUFS				45

#define ACL_BUF_SIZE				1800
#define CTRL_BUF_SIZE				660

#define ROUNDUP32(v)				(((u32)(v)+0x1f)&~0x1f)
#define ROUNDDOWN32(v)				(((u32)(v)-0x1f)&~0x1f)

struct usbtxbuf
{
	u32 txsize;
	void *rpData;
};

static struct _usb_p __usbdev ALIGNED(32);
static struct ipcmessage intrmsg ALIGNED(32);
static struct ipcmessage bulkmsg ALIGNED(32);

vu32 intr = 0, bulk = 0;
vs32 intrres = 0, bulkres = 0;
s32 intrqueue = -1, bulkqueue = -1;

static struct memb_blks ctrlbufs ALIGNED(32);
static struct memb_blks aclbufs ALIGNED(32);
struct usbtxbuf *intrdata = NULL, *bulkdata = NULL;

#define USB_IOCTL_CTRLMSG				0
#define USB_IOCTL_BLKMSG				1
#define USB_IOCTL_INTRMSG				2

static inline s32 __usb_control_message(s32 fd,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *rpData)
{
	s32 ret = -12;

	if(((s32)rpData%32)!=0) return -22;
	if(wLength && !rpData) return -22;
	if(!wLength && rpData) return -22;

	u8 *pRqType = NULL,*pRq = NULL,*pNull = NULL;
	u16 *pValue = NULL,*pIndex = NULL,*pLength = NULL;
	ioctlv *vec = (ioctlv*)malloca(sizeof(ioctlv)*7,32);

	pRqType = (u8*)malloca(32,32);
	*pRqType  = bmRequestType;

	pRq = (u8*)malloca(32,32);
	*pRq = bmRequest;

	pValue = (u16*)malloca(32,32);
	*pValue = bswap16(wValue);

	pIndex = (u16*)malloca(32,32);
	*pIndex = bswap16(wIndex);

	pLength = (u16*)malloca(32,32);
	*pLength = bswap16(wLength);

	pNull = (u8*)malloca(32,32);
	*pNull = 0;

	vec[0].data = pRqType;
	vec[0].len = sizeof(u8);
	vec[1].data = pRq;
	vec[1].len = sizeof(u8);
	vec[2].data = pValue;
	vec[2].len = sizeof(u16);
	vec[3].data = pIndex;
	vec[3].len = sizeof(u16);
	vec[4].data = pLength;
	vec[4].len = sizeof(u16);
	vec[5].data = pNull;
	vec[5].len = sizeof(u8);
	vec[6].data = rpData;
	vec[6].len = wLength;

	//dbgprintf("IOS_Ioctlv ctrl\n");
	ret = IOS_Ioctlv(fd,USB_IOCTL_CTRLMSG,6,1,vec);

	free(vec);
	free(pRqType);
	free(pRq);
	free(pValue);
	free(pIndex);
	free(pLength);
	free(pNull);

	return ret;
}

static inline s32 __usb_interrupt_bulk_message(s32 fd,u8 ioctl,u8 bEndpoint,u16 wLength,void *rpData)
{
	s32 ret = -12;

	if(((s32)rpData%32)!=0) return -22;
	if(wLength && !rpData) return -22;
	if(!wLength && rpData) return -22;

	u8 *pEndP = NULL;
	u16 *pLength = NULL;

	ioctlv *vec = (ioctlv*)malloca(sizeof(ioctlv)*3,32);

	pEndP = (u8*)malloca(32,32);
	*pEndP  = bEndpoint;

	pLength = (u16*)malloca(32,32);
	*pLength = wLength;

	vec[0].data = pEndP;
	vec[0].len = sizeof(u8);
	vec[1].data = pLength;
	vec[1].len = sizeof(u16);
	vec[2].data = rpData;
	vec[2].len = wLength;

	//dbgprintf("IOS_Ioctlv bulk\n");
	ret = IOS_Ioctlv(fd,ioctl,2,1,vec);

	free(vec);
	free(pEndP);
	free(pLength);

	return ret;
}

s32 __readbulkdataCB()
{
	u8 *ptr;
	u32 len;
	struct pbuf *p,*q;

	if(__usbdev.openstate!=0x0002) return 0;

	if(bulkres>0) {
		len = bulkres;
		//dbgprintf("%08x\n",bulkres);
		p = btpbuf_alloc(PBUF_RAW,len,PBUF_POOL);
		if(p!=NULL) {
			ptr = bulkdata->rpData;
			for(q=p;q!=NULL && len>0;q=q->next) {
				memcpy(q->payload,ptr,q->len);
				ptr += q->len;
				len -= q->len;
			}
			hci_acldata_handler(p);
			//SYS_SwitchFiber((u32)p,0,0,0,(u32)hci_acldata_handler,(u32)(&__ppc_btstack2[STACKSIZE]));
			btpbuf_free(p);
		} else
			ERROR("__readbulkdataCB: Could not allocate memory for pbuf.\n");
	}
	btmemb_free(&aclbufs,bulkdata);
	//return __issue_bulkread();
	return 0;
}

s32 __readintrdataCB()
{
	u8 *ptr;
	u32 len;
	struct pbuf *p,*q;

	if(__usbdev.openstate!=0x0002) return 0;

	if(intrres>0) {
		len = intrres;
		//dbgprintf("%08x\n",intrres);
		p = btpbuf_alloc(PBUF_RAW,len,PBUF_POOL);
		if(p!=NULL) {
			ptr = intrdata->rpData;
			for(q=p;q!=NULL && len>0;q=q->next) {
				memcpy(q->payload,ptr,q->len);
				ptr += q->len;
				len -= q->len;
			}
			hci_event_handler(p);
			//SYS_SwitchFiber((u32)p,0,0,0,(u32)hci_event_handler,(u32)(&__ppc_btstack1[STACKSIZE]));
			btpbuf_free(p);
		} else
			ERROR("__readintrdataCB: Could not allocate memory for pbuf.\n");
	}
	btmemb_free(&ctrlbufs,intrdata);
	//return __issue_intrread();
	return 0;
}

u8 intrpEndP[32] ALIGNED(32);
u16 intrpLength[16] ALIGNED(32);
ioctlv intrvec[3] ALIGNED(32);

u32 __issue_intrread()
{
	u32 len;
	u8 *ptr;

	if(__usbdev.openstate!=0x0002) return 0;

	intrdata = (struct usbtxbuf*)btmemb_alloc(&ctrlbufs);
	if(intrdata != NULL)
	{
		ptr = (u8*)((u32)intrdata + sizeof(struct usbtxbuf));
		intrdata->rpData = (void*)ROUNDUP32(ptr);
		len = (ctrlbufs.size - ((u32)intrdata->rpData - (u32)intrdata));
		intrdata->txsize = ROUNDDOWN32(len);
		*intrpEndP = __usbdev.hci_evt;
		*intrpLength = intrdata->txsize;
		//ret = USB_ReadIntrMsgAsync(__usbdev.fd,__usbdev.hci_evt,buf->txsize,buf->rpData,__readintrdataCB,buf);
		intrvec[0].data = intrpEndP;
		intrvec[0].len = sizeof(u8);
		intrvec[1].data = intrpLength;
		intrvec[1].len = sizeof(u16);
		intrvec[2].data = intrdata->rpData;
		intrvec[2].len = intrdata->txsize;
		//dbgprintf("IOS_IoctlvAsync Bulk %08x %08x %08x\n", intrpEndP, intrpLength, intrdata->rpData);
		IOS_IoctlvAsync(__usbdev.fd,USB_IOCTL_INTRMSG,2,1,intrvec,intrqueue,&intrmsg);
	}

	return 0;
}

static u32 intrreadAlarm()
{
	struct ipcmessage *msg = NULL;
	while(1)
	{
		mqueue_recv(intrqueue, &msg, 0);
		intrres = msg->result;
		mqueue_ack(msg, 0);
		intr = 1;
	}
	return 0;
}

static u32 bulkreadAlarm()
{
	struct ipcmessage *msg = NULL;
	while(1)
	{
		mqueue_recv(bulkqueue, &msg, 0);
		bulkres = msg->result;
		mqueue_ack(msg, 0);
		bulk = 1;
	}
	return 0;
}

u8 bulkpEndP[32] ALIGNED(32);
u16 bulkpLength[16] ALIGNED(32);
ioctlv bulkvec[3] ALIGNED(32);

u32 __issue_bulkread()
{
	u32 len;
	u8 *ptr;

	if(__usbdev.openstate!=0x0002) return 0;
	bulkdata = (struct usbtxbuf*)btmemb_alloc(&aclbufs);
	if(bulkdata != NULL)
	{
		ptr = (u8*)((u32)bulkdata + sizeof(struct usbtxbuf));
		bulkdata->rpData = (void*)ROUNDUP32(ptr);
		len = (aclbufs.size - ((u32)bulkdata->rpData - (u32)bulkdata));
		bulkdata->txsize = ROUNDDOWN32(len);
		*bulkpEndP = __usbdev.acl_in;
		*bulkpLength = bulkdata->txsize;
		//ret = USB_ReadBlkMsgAsync(__usbdev.fd,__usbdev.acl_in,buf->txsize,buf->rpData,__readbulkdataCB,buf);
		bulkvec[0].data = bulkpEndP;
		bulkvec[0].len = sizeof(u8);
		bulkvec[1].data = bulkpLength;
		bulkvec[1].len = sizeof(u16);
		bulkvec[2].data = bulkdata->rpData;
		bulkvec[2].len = bulkdata->txsize;
		//dbgprintf("IOS_IoctlvAsync Bulk %08x %08x %08x\n", bulkpEndP, bulkpLength, bulkdata->rpData);
		IOS_IoctlvAsync(__usbdev.fd,USB_IOCTL_BLKMSG,2,1,bulkvec,bulkqueue,&bulkmsg);
	}

	return 0;
}

static s32 __initUsbIOBuffer(struct memb_blks *blk,u32 buf_size,u32 num_bufs)
{
	u32 len;
	u8 *ptr = NULL;

	len = ((MEM_ALIGN_SIZE(buf_size)+sizeof(u32))*num_bufs);
	ptr = (u8*)malloca(len, 32);
	//dbgprintf("%08x %08x\n", ptr, len);

	blk->size = buf_size;
	blk->num = num_bufs;
	blk->mem = ptr;

	btmemb_init(blk);
	return 0;
}

static s32 __getDeviceId(u16 vid,u16 pid)
{
	char *devicepath = malloca(32,32);
	_sprintf(devicepath,"/dev/usb/oh1/%x/%x",vid,pid);
	__usbdev.fd = IOS_Open(devicepath,0);
	free(devicepath);

	//dbgprintf("__getDeviceId(%04x,%04x,%d)\n",vid,pid,__usbdev.fd);
	return __usbdev.fd;
}

static s32 __usb_register()
{
	s32 ret = 0;

	memset(&__usbdev,0,sizeof(struct _usb_p));
	__usbdev.openstate = 5;

	__usbdev.vid = 0x057E;
	__usbdev.pid = 0x0305;

	ret = __getDeviceId(__usbdev.vid,__usbdev.pid);
	if(ret<0) return ret;

	__usbdev.acl_out		= 0x02;
	__usbdev.acl_in			= 0x82;
	__usbdev.hci_evt		= 0x81;
	__usbdev.hci_ctrl		= 0x00;

	__initUsbIOBuffer(&ctrlbufs,CTRL_BUF_SIZE,NUM_CTRL_BUFS);
	__initUsbIOBuffer(&aclbufs,ACL_BUF_SIZE,NUM_ACL_BUFS);

	__usbdev.openstate = 4;

	return ret;
}

static u32 Bulkread_Thread = 0, Interruptread_Thread = 0;
u8 *bulkheap = NULL, *intrheap = NULL;
extern char __intr_stack_addr, __intr_stack_size;
extern char __blk_stack_addr, __blk_stack_size;
static s32 __usb_open(pbcallback cb)
{
	if(__usbdev.openstate!=0x0004) return -1;

	__usbdev.openstate = 2;

	intrheap = (u8*)malloca(32,32);
	intrqueue = mqueue_create(intrheap, 1);

	bulkheap = (u8*)malloca(32,32);
	bulkqueue = mqueue_create(bulkheap, 1);

	Interruptread_Thread = do_thread_create(intrreadAlarm, ((u32*)&__intr_stack_addr), ((u32)(&__intr_stack_size)), 0x78);
	thread_continue(Interruptread_Thread);
	mdelay(100);
	//dbgprintf("Started intrreadAlarm thread\n");

	Bulkread_Thread = do_thread_create(bulkreadAlarm, ((u32*)&__blk_stack_addr), ((u32)(&__blk_stack_size)), 0x78);
	thread_continue(Bulkread_Thread);
	mdelay(100);
	//dbgprintf("Started bulkreadAlarm thread\n");

	__issue_intrread();

	__issue_bulkread();

	return 0;
}

void physbusif_init()
{
	s32 ret;

	ret = __usb_register();
	if(ret<0) return;

	__usb_open(NULL);
}

void physbusif_close()
{
	if(__usbdev.openstate!=0x0002) return;

	__usbdev.openstate = 4;
}

void physbusif_shutdown()
{
	if(__usbdev.openstate!=0x0004) return;
	IOS_Close(__usbdev.fd);
	__usbdev.fd = -1;
}

void physbusif_reset_all()
{
	return;
}

void physbusif_output(struct pbuf *p,u16_t len)
{
	u32 pos;
	u8 *ptr;
	struct pbuf *q;
	struct memb_blks *mblks;
	struct usbtxbuf *blkbuf;

	if(__usbdev.openstate!=0x0002) return;

	if(((u8*)p->payload)[0]==HCI_COMMAND_DATA_PACKET) mblks = &ctrlbufs;
	else if(((u8*)p->payload)[0]==HCI_ACL_DATA_PACKET) mblks = &aclbufs;
	else return;

	blkbuf = btmemb_alloc(mblks);
	if(blkbuf!=NULL) {
		blkbuf->txsize = --len;
		blkbuf->rpData = (void*)ROUNDUP32(((u32)blkbuf+sizeof(struct usbtxbuf)));

		ptr = blkbuf->rpData;
		for(q=p,pos=1;q!=NULL && len>0;q=q->next,pos=0) {
			memcpy(ptr,q->payload+pos,(q->len-pos));
			ptr += (q->len-pos);
			len -= (q->len-pos);
		}

		/*if(((u8*)p->payload)[0]==HCI_COMMAND_DATA_PACKET) {
			USB_WriteCtrlMsgAsync(__usbdev.fd,0x20,0,0,0,blkbuf->txsize,blkbuf->rpData,__writectrlmsgCB,blkbuf);
		} else if(((u8*)p->payload)[0]==HCI_ACL_DATA_PACKET) {
			USB_WriteBlkMsgAsync(__usbdev.fd,__usbdev.acl_out,blkbuf->txsize,blkbuf->rpData,__writebulkmsgCB,blkbuf);
		}*/
		if(((u8*)p->payload)[0]==HCI_COMMAND_DATA_PACKET) {
			__usb_control_message(__usbdev.fd,0x20,0,0,0,blkbuf->txsize,blkbuf->rpData);
		} else if(((u8*)p->payload)[0]==HCI_ACL_DATA_PACKET) {
			__usb_interrupt_bulk_message(__usbdev.fd,USB_IOCTL_BLKMSG,__usbdev.acl_out,blkbuf->txsize,blkbuf->rpData);
		}
		btmemb_free(mblks,blkbuf);
	}
}
