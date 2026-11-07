"""Execute actual HID.c parser/read/poll functions as big-endian ARM code.

IOS, timer, LED and cache operations are stubbed. This checks bounds and state
transitions, not physical USB timing, hotplug or cache coherency.
"""
import os, re, struct, subprocess
from pathlib import Path
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_MODE_BIG_ENDIAN, UC_HOOK_MEM_READ
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R0, UC_ARM_REG_PC

base=Path(__file__).resolve().parent
source=(base.parents[1]/'kernel/HID.c').read_text(encoding='utf-8')
build=base/'build';build.mkdir(exist_ok=True)
prefix=str(Path(os.environ['DEVKITARM'])/'bin/arm-none-eabi-')
suffix='.exe' if os.name=='nt' else ''
def tool(name,*args):
    return subprocess.check_output([prefix+name+suffix,*map(str,args)],text=True)
def function(name):
    match=re.search(r'(?:static )?(?:bool|u8|void) '+name+r'\([^;]*?\)\s*\{',source)
    assert match,name
    at=source.index('{',match.start());level=1;end=at+1
    while level:
        level+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[match.start():end].replace('static ','',1)
defines='\n'.join(re.findall(r'^#define XINPUT_\w+[^\n]*',source,re.M))
fixture=r'''
typedef unsigned int u32;typedef int s32;typedef unsigned char u8;
typedef short s16;typedef unsigned short u16;typedef int bool;
#define true 1
#define false 0
#define NULL ((void*)0)
#define USB_DT_DEVICE 1
#define USB_DT_CONFIG 2
#define USB_DT_INTERFACE 4
#define USB_DT_ENDPOINT 5
#define USB_DT_DEVICE_SIZE 18
#define USB_DT_CONFIG_SIZE 9
#define USB_DT_INTERFACE_SIZE 9
#define USB_DT_ENDPOINT_SIZE 7
#define USB_ENDPOINT_INTERRUPT 3
#define USB_ENDPOINT_IN 128
#define HID_STATUS 0x13003440u
#define HW_TIMER 0x0D800010u
#define GetDeviceChange 1
#define AttachFinish 6
typedef struct {u32 data[3];} usb_device_entry;
usb_device_entry XInputDevices[32];
u32 XInputEpIn,XInputEpOut,wMaxPacketSize,MemPacketSize=32;
u8 packet[32],XInputCooked[32];u8 *XInputPacket=packet;
u8 *HID_Packet=(u8*)0x13005000u;
u32 XInputReadLen=32,XInputArmed=1,XInputActive,XInputWaitTimer;
u32 XInputErrors,XInputSpikeRun,XInputHavePrev,XInputReadTimer,XInputResubmit;
s16 XInPrevLX,XInPrevLY,XInPrevRX,XInPrevRY;
s32 XInputHandle=1,XInputLastResult;
u32 ControllerID=1,xinchange,xinattach,xinread;
void *xinreadmsg,*xinchangemsg,*xinattachmsg;int hidqueue;
u32 ticks=20000,transfers,changes,leds;s32 submitResult;
void XInputRead(void);void (*HIDRead)(void)=XInputRead;
void *memset32(void*p,int v,u32 n){u8*d=p;while(n--)*d++=v;return p;}
void *memcpy(void*d,const void*s,u32 n){u8*a=d;const u8*b=s;while(n--)*a++=*b++;return d;}
void sync_before_read(void*p,u32 n){} void sync_after_write(void*p,u32 n){}
u32 read32(u32 p){return ticks;}void write32(u32 p,u32 v){*(u32*)p=v;}
u32 TimerDiffTicks(u32 t){return ticks-t;}
s32 IOS_IoctlAsync(int fd,int cmd,void*a,int b,void*c,int d,int q,void*m){changes++;return 0;}
s32 XInputTransfer(u8*p,u32 n,u32 ep,void*m){transfers++;return submitResult;}
s32 XInputSetLED(u8 n){leds++;return 0;}bool XInputOpen(void){return false;}
'''+defines+'\n'+'\n'.join(function(n) for n in [
    'XInputDescriptor','XInputParseDescriptors','XInputAxisSpiked',
    'XInputNormalizeAxis','XInputRead','XInputUpdate'])
(build/'fixture.c').write_text(fixture,encoding='utf-8')
tool('gcc','-O2','-mbig-endian','-mcpu=arm926ej-s','-ffreestanding','-fno-builtin',
     '-nostdlib','-Wl,-Ttext=0x12000000','-Wl,-e,XInputUpdate',build/'fixture.c','-o',build/'fixture.elf')
tool('objcopy','-O','binary',build/'fixture.elf',build/'fixture.bin')
symbols={v[2]:int(v[0],16) for line in tool('nm',build/'fixture.elf').splitlines() if len(v:=line.split())==3}
pack=lambda v:struct.pack('>I',v&0xffffffff)
def machine():
    u=Uc(UC_ARCH_ARM,UC_MODE_ARM|UC_MODE_BIG_ENDIAN)
    for a,n in [(0x12000000,0x40000),(0x12100000,0x10000),(0x13000000,0x10000)]:u.mem_map(a,n)
    u.mem_write(0x12000000,(build/'fixture.bin').read_bytes())
    return u
def setv(u,n,v):u.mem_write(symbols[n],pack(v))
def get(u,n):return struct.unpack('>I',u.mem_read(symbols[n],4))[0]
def call(u,n,arg=0):
    u.reg_write(UC_ARM_REG_SP,0x1210f000);u.reg_write(UC_ARM_REG_LR,0x12100000)
    u.reg_write(UC_ARM_REG_R0,arg);u.emu_start(symbols[n],0x12100000,count=200000)
    assert u.reg_read(UC_ARM_REG_PC)==0x12100000,n
    return u.reg_read(UC_ARM_REG_R0)
def descriptors():
    b=bytearray(192);b[20:22]=bytes([18,1]);b[40:42]=bytes([9,2])
    b[52:61]=bytes([9,4,0,0,2,255,93,1,0])
    b[64:71]=bytes([7,5,129,3,0,32,1]);b[72:79]=bytes([7,5,1,3,0,32,1])
    return b
tests=0
def parse(b,expected):
    global tests
    u=machine();p=0x12101000;u.mem_write(p,bytes(b))
    def bound(uc,access,a,n,v,data):
        if p<=a<p+0x1000:assert a+n<=p+192,('descriptor overread',hex(a))
    u.hook_add(UC_HOOK_MEM_READ,bound)
    assert call(u,'XInputParseDescriptors',p)==expected
    if expected:assert get(u,'XInputEpIn')==129 and get(u,'wMaxPacketSize')==32
    tests+=1
parse(descriptors(),1)
for offset,value in [(20,255),(20,0),(21,9),(40,255),(40,0),(52,255),(52,0),
                     (54,1),(57,3),(58,0),(59,0),(64,0),(64,255),(64,6),
                     (65,4),(67,2),(69,19),(69,64),(72,255),(56,30)]:
    b=descriptors();b[offset]=value;parse(b,0)
# A bounded vendor-specific descriptor before the endpoints is valid.
b=descriptors();b[68:84]=b[64:80];b[64:68]=bytes([4,33,0,0]);parse(b,1)
# Exhaust each potentially dangerous one-byte length with an input-read guard.
for offset in [20,40,52,64,72]:
    for value in range(193,256):
        b=descriptors();b[offset]=value;parse(b,0)
print('PASS',tests,'bounded descriptor cases')

report=bytearray(32);report[1]=20;report[3]=128
for n in range(20):
    u=machine();u.mem_write(symbols['packet'],bytes(report));setv(u,'XInputLastResult',n)
    u.mem_write(0x13005000,b'\x5a'*32);call(u,'XInputRead')
    assert bytes(u.mem_read(0x13005000,32))==b'\x5a'*32 and not get(u,'XInputActive')
print('PASS short reports do not publish stale buttons or axes')
u=machine();u.mem_write(symbols['packet'],bytes(report));setv(u,'XInputLastResult',20)
call(u,'XInputRead');assert bytes(u.mem_read(0x13005003,7))==bytes([128,0,0,128,127,128,127])
assert get(u,'XInputActive') and get(u,'XInputResubmit')
report[6:8]=bytes([255,127]);u.mem_write(symbols['packet'],bytes(report))
for _ in range(8):call(u,'XInputRead');assert u.mem_read(0x13005006,1)==b'\x80'
call(u,'XInputRead');assert u.mem_read(0x13005006,1)==b'\xff'
print('PASS complete report normalization and bounded spike hold')

u=machine();setv(u,'XInputResubmit',1);setv(u,'submitResult',-4)
for n in range(201):
    setv(u,'ticks',(n+1)*20000);call(u,'XInputUpdate')
assert get(u,'transfers')==201 and get(u,'changes')==1
assert not get(u,'XInputResubmit') and not get(u,'XInputArmed')
setv(u,'ticks',5000000);call(u,'XInputUpdate');assert get(u,'transfers')==201
print('PASS immediate submission errors retry, then stop and re-enumerate')
u=machine();setv(u,'XInputResubmit',1);setv(u,'ticks',15000);call(u,'XInputUpdate')
assert not get(u,'transfers');setv(u,'ticks',16000);call(u,'XInputUpdate')
assert get(u,'transfers')==1 and not get(u,'XInputResubmit')
setv(u,'ticks',32000);call(u,'XInputUpdate');assert get(u,'transfers')==1
print('PASS poll spacing and exactly one outstanding transfer')
