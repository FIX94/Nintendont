"""Compile the actual VI candidate case and exercise bounds/rejection on ARM.
The scanner's statistical matching is outside this fixture; its validation and
Found/patch-allocation side effects are tested directly from Patch.c.
"""
import os, re, struct, subprocess, json
from pathlib import Path
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_MODE_BIG_ENDIAN, UC_HOOK_MEM_READ
from unicorn.arm_const import *
base=Path(__file__).resolve().parent
src=Path(os.environ.get('NIN_SOURCE',str(base.parents[1])))
prefix=str(Path(os.environ['DEVKITARM'])/'bin/arm-none-eabi-')
suffix='.exe' if os.name=='nt' else ''
def run(tool,*args):
    return subprocess.check_output([prefix+tool+suffix,*map(str,args)],text=True)
s=(src/'kernel/Patch.c').read_text()
case=s[s.index('case FCODE_VIRetraceHandler:'):s.index('case FCODE_PADRead:',s.index('case FCODE_VIRetraceHandler:'))]
fixture=r'''
typedef unsigned int u32; typedef unsigned char u8; typedef int s32;
typedef struct {u32 Length;const char *Type;u32 Found;const char *Name;} Pattern;
#define FCODE_VIRetraceHandler 1
#define OVL_STATE_ARM 0x1318F000u
static const char types[]="ABCDEFGHIJ";
u32 POffset,VIHook_size=108,VIRetraceAddr,VIRetraceLen,VIHookStub;
const char *VIRetraceVariant;static const u8 VIHook[108]={0};
u32 allocations,installs;
u32 read32(u32 a){return *(volatile u32*)a;}
void write32(u32 a,u32 v){*(volatile u32*)a=v;}
void sync_before_read(void*a,u32 n){} void sync_after_write(void*a,u32 n){}
void printpatchfound(const char*a,const char*b,u32 n){}
u32 PatchCopy(const u8*p,u32 n){allocations++;return POffset-=n;}
void PatchB(u32 dst,u32 site){write32(site,0x48000000|((dst-site)&0x03fffffc));}
void OverlayInstallCode(void){installs++;}
u32 candidate(u32 *args){
 char *Buffer=(char*)args[0];u32 Length=args[1],FOffset=args[2],j=0;
 Pattern p={args[3],&types[args[4]],FOffset};Pattern *CurPatterns=&p;
 POffset=args[5];allocations=0;installs=0;
 switch(1){
'''+case+r'''
 }
 args[6]=allocations;args[7]=installs;args[8]=POffset;
 return p.Found;
}
'''
tmp=base/'patch-fixture';tmp.mkdir(exist_ok=True)
(tmp/'case.c').write_text(fixture)
run('gcc','-O2','-mbig-endian','-mcpu=arm926ej-s','-ffreestanding','-fno-builtin',
    '-nostdlib','-Wl,-Ttext=0x12000000','-Wl,-e,candidate',tmp/'case.c','-o',tmp/'case.elf')
run('objcopy','-O','binary',tmp/'case.elf',tmp/'case.bin')
syms={p[2]:int(p[0],16) for line in run('nm',tmp/'case.elf').splitlines() if len(p:=line.split())==3}
pack=lambda v:struct.pack('>I',v&0xffffffff)
basefn=0x8000;results=[]
def probe(name,variant=0,length=0x1dc,available=None,bad=None,space=0x2ff4,expect=False,addr=basefn):
    uc=Uc(UC_ARCH_ARM,UC_MODE_ARM|UC_MODE_BIG_ENDIAN)
    for a,n in [(0,0x10000),(0x12000000,0x20000),(0x12100000,0x10000),(0x1318f000,0x1000)]:uc.mem_map(a,n)
    uc.mem_write(0x12000000,(tmp/'case.bin').read_bytes())
    uc.mem_write(addr,pack(0x7c0802a6));tail=addr+length
    uc.mem_write(tail-8,pack(0x7c0803a6));uc.mem_write(tail-4,pack(0x38210020));uc.mem_write(tail,pack(0x4e800020))
    calls=[(45,61,114),(45,61,115),(47,62,132),(39,47,126),(39,47,131),
           (42,50,132),(42,50,134),(47,55,139),(44,59,151),(49,64,156)]
    if variant<10:
        for k,index in enumerate(calls[variant]):
            site=addr+4*index;dst=0x7000+(4 if bad=='target' and k==2 else 0)
            uc.mem_write(site,pack(0x48000001|((dst-site)&0x03fffffc)))
    if bad=='prologue':uc.mem_write(addr,pack(0x60000000))
    if bad=='epilogue':uc.mem_write(tail,pack(0x60000000))
    available=length+4 if available is None else available
    args=[addr,available,addr,length,variant,space,0,0,0]
    uc.mem_write(0x12100100,b''.join(map(pack,args)))
    reads=[]
    def bounded(uc,access,a,n,v,data):
        if addr<=a<addr+0x1000:
            assert a+n<=addr+available,('read past supplied scan range',name,hex(a))
            reads.append(a)
    uc.hook_add(UC_HOOK_MEM_READ,bounded)
    uc.reg_write(UC_ARM_REG_SP,0x1210ff00);uc.reg_write(UC_ARM_REG_R0,0x12100100);uc.reg_write(UC_ARM_REG_LR,0x12100000)
    uc.emu_start(syms['candidate'],0x12100000,count=10000)
    assert uc.reg_read(UC_ARM_REG_PC)==0x12100000
    found=uc.reg_read(UC_ARM_REG_R0)
    after=struct.unpack('>9I',uc.mem_read(0x12100100,36))
    assert found==(addr if expect else 0),(name,hex(found))
    assert after[6:8]==((1,1) if expect else (0,0)),(name,after)
    if not expect:assert after[8]==space,'rejection consumed patch pool'
    results.append(name)
for v,n in enumerate([0x1dc,0x1e0,0x224,0x20c,0x220,0x224,0x22c,0x248,0x270,0x28c]):
    probe('valid variant '+chr(65+v),variant=v,length=n,expect=True)
probe('reject mismatching context call',bad='target')
probe('reject prologue collision',bad='prologue')
probe('reject epilogue collision',bad='epilogue')
probe('reject truncated scan',available=0x100)
probe('reject invalid variant',variant=10)
probe('reject exhausted patch pool',space=0x1810)
print(json.dumps({'passed':results,'case_source':'kernel/Patch.c: FCODE_VIRetraceHandler'},indent=2))
