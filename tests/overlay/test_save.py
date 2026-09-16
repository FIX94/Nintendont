import os
from pathlib import Path
import sys,struct,subprocess,json
base=Path(__file__).resolve().parent;# Install unicorn from requirements.txt before running.
from unicorn import *
from unicorn.ppc_const import *
from unicorn.arm_const import *
arm=str(Path(os.environ['DEVKITARM'])/'bin/arm-none-eabi-')
pack=lambda n:struct.pack('>I',n&0xffffffff)
def symbols(elf):
    out=subprocess.check_output([arm+'nm'+('.exe' if os.name=='nt' else ''),str(elf)],text=True)
    return {p[2]:int(p[0],16) for line in out.splitlines() if len(p:=line.split())==3 and p[1].lower()!='u'}
reports=[]
# Audit actual stores on the real M17 and M19 opening input paths.
for ver in ['m21']:
    uc=Uc(UC_ARCH_PPC,UC_MODE_32|UC_MODE_BIG_ENDIAN)
    for a,n in [(0x93180000,0x10000),(0xD318F000,0x1000),(0x81000000,0x20000),(0x80002000,0x1000)]:uc.mem_map(a,n)
    uc.mem_write(0x93180000,(Path(os.environ.get('NIN_SOURCE',str(base.parents[1])))/'overlay/overlay.bin').read_bytes())
    s=bytearray(224 if ver=='m17' else 868)
    s[:8]=pack(0x4F564C35 if ver=='m17' else 0x4F563231)+pack(1)
    identity=bytes(list(range(12))+[0]*8)*4
    if ver=='m17':s[64:144]=identity;s[144:224]=identity
    else:s[72:392]=b''.join(pack(v) for v in identity);s[392:712]=s[72:392]
    uc.mem_write(0xD318F000,bytes(s));uc.mem_write(0x81001000,b'\x1c\x00'+bytes(46))
    uc.reg_write(UC_PPC_REG_1,0x81010000);uc.reg_write(UC_PPC_REG_3,0x81001000)
    uc.reg_write(UC_PPC_REG_4,1);uc.reg_write(UC_PPC_REG_LR,0x80002FFC)
    bad=[]
    def audit(uc,access,a,n,v,data):
        if n!=4 or a%4:bad.append({'address':hex(a),'bytes':n})
    uc.hook_add(UC_HOOK_MEM_WRITE,audit,begin=0xD0000000,end=0xD3FFFFFF)
    uc.emu_start(0x93180000,0x80002FFC,count=100000)
    assert uc.reg_read(UC_PPC_REG_PC)==0x80002FFC
    assert bool(bad)==(ver=='m17')
    reports.append({'build':ver,'unsafe_uncached_stores_on_open':len(bad),'examples':bad[:4]})

# FAT is stubbed in memory, but OverlayInit/Save and all packing/checksum code
# below are executed from the actual ARM kernel binary. No USB writes occur.
src=Path(os.environ.get('NIN_SOURCE',str(base.parents[1])));syms=symbols(src/'kernel/kernel.elf')
tmp=base/'m21-save-test';tmp.mkdir(exist_ok=True)
(tmp/'layout.c').write_text('#include "%s"\nconst unsigned int layout[]={__builtin_offsetof(FIL,obj.objsize),sizeof(((FIL*)0)->obj.objsize)};\n'%(src/'fatfs/ff.h').as_posix())
subprocess.run([arm+'gcc'+('.exe' if os.name=='nt' else ''),'-mbig-endian','-mcpu=arm926ej-s','-c',str(tmp/'layout.c'),'-o',str(tmp/'layout.o')],check=True)
subprocess.run([arm+'objcopy'+('.exe' if os.name=='nt' else ''),'-O','binary','-j','.rodata',str(tmp/'layout.o'),str(tmp/'layout.bin')],check=True)
sizeoff,sizebytes=struct.unpack('>II',(tmp/'layout.bin').read_bytes())
def checksum(b):
    h=2166136261
    for i,v in enumerate(b):
        if not 12<=i<16:h=((h^v)*16777619)&0xffffffff
    return h
def saved(m,gen):
    b=bytearray(struct.pack('>4I',0x4D31354D,1,gen,0)+bytes(m));b[12:16]=pack(checksum(b));return bytes(b)
slot0='/ninmap-m15-0.bin';slot1='/ninmap-m15-1.bin'
class Arm:
    def __init__(self,files,fail=None):
        self.files=dict(files);self.open={};self.writes=0;self.fail=fail;self.write_handles=set()
        self.uc=uc=Uc(UC_ARCH_ARM,UC_MODE_ARM|UC_MODE_BIG_ENDIAN)
        for a,n in [(0x12F00000,0x100000),(0x13180000,0x10000),(0x12000000,0x1000)]:uc.mem_map(a,n)
        uc.mem_write(0x12F00000,(src/'kernel/kernel.bin').read_bytes())
        self.names={syms[name]:name for name in ['f_open_char','f_read','f_write','f_close','f_sync','sync_after_write','sync_before_read']}
        uc.hook_add(UC_HOOK_CODE,self.hook)
    def hook(self,uc,a,size,data):
        name=self.names.get(a)
        if name is None:return
        r=[uc.reg_read(reg) for reg in [UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3]];result=0
        if name=='f_open_char':
            path=bytes(uc.mem_read(r[1],64)).split(b'\0')[0].decode()
            if self.fail=='open' and r[2]&8:
                uc.reg_write(UC_ARM_REG_R0,1);uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR));return
            if self.fail=='reopen' and self.writes and not r[2]&8:
                uc.reg_write(UC_ARM_REG_R0,1);uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR));return
            if r[2]&8:self.files[path]=b''
            if path not in self.files:result=4
            else:
                self.open[r[0]]=path
                if r[2]&8:self.write_handles.add(r[0])
                else:self.write_handles.discard(r[0])
                uc.mem_write(r[0]+sizeoff,len(self.files[path]).to_bytes(sizebytes,'big'))
        elif name=='f_read':
            b=self.files[self.open[r[0]]][:r[2]];uc.mem_write(r[1],b);uc.mem_write(r[3],pack(len(b)))
            if self.fail=='readback' and self.writes:uc.mem_write(r[1],b'BAD!')
        elif name=='f_write':
            n=48 if self.fail=='short' else r[2]
            self.files[self.open[r[0]]]=bytes(uc.mem_read(r[1],n));uc.mem_write(r[3],pack(n));self.writes+=1
            if self.fail=='write':result=1
        elif name=='f_sync' and self.fail=='sync':result=1
        elif name=='f_close' and self.fail=='close' and r[0] in self.write_handles:result=1
        uc.reg_write(UC_ARM_REG_R0,result);uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
    def call(self,name):
        uc=self.uc;uc.reg_write(UC_ARM_REG_SP,0x12FE2000);uc.reg_write(UC_ARM_REG_LR,0x12000000)
        uc.emu_start(syms[name],0x12000000,count=300000)
        assert uc.reg_read(UC_ARM_REG_PC)==0x12000000
        return uc.reg_read(UC_ARM_REG_R0)&0xffffffff
    def maps(self):return list(struct.unpack('>80I',self.uc.mem_read(0x1318F048,320)))
m=[]
for p in range(4):m+=list((i+p)%13 for i in range(12))+[p%2]*5+[0]*3
a=Arm({slot0:saved(m,7)})
canary=b'\xa5'*0x10000;a.uc.mem_write(0x13180000,canary)
a.call('OverlayInit')
assert bytes(a.uc.mem_read(0x13180000,len(canary)))==canary,'init overwrote live loader heap'
assert a.call('OverlaySave')==0,'no hook install must mean no shared-state save'
a.call('OverlayInstallCode')
installed=bytes(a.uc.mem_read(0x13180000,0x10000))
a.call('OverlayInstallCode')
assert bytes(a.uc.mem_read(0x13180000,0x10000))==installed,'install must be idempotent'
assert a.maps()==m and bytes(a.uc.mem_read(0x1318F188,320))==bytes(a.uc.mem_read(0x1318F048,320))
assert a.uc.mem_read(0x1318F024,4)==bytes(4),'startup banner must remain disabled'
assert a.call('OverlaySave')==0 and a.writes==0
a.uc.mem_write(0x1318F01C,pack(1)) # dirty
assert a.call('OverlaySave')==1 and a.writes==1
assert a.files[slot1]==saved(m,8) and a.files[slot0]==saved(m,7)
b=Arm(a.files);b.call('OverlayInit');b.call('OverlayInstallCode');assert b.maps()==m
assert b.uc.mem_read(0x1318F020,4)==pack(8)
b.uc.mem_write(0x1318F01C,pack(1));b.uc.mem_write(0x1318F048,pack(256))
assert b.call('OverlaySave')==0xffffffff and b.writes==0,'out-of-range words must not truncate into valid bytes'
c=Arm({});c.call('OverlayInit');c.call('OverlayInstallCode');assert c.maps()==list(identity)
assert a.uc.mem_read(0x1318F01C,4)==pack(0) and a.uc.mem_read(0x1318F020,4)==pack(8)
assert a.call('OverlaySave')==0 and a.writes==1,'unchanged repeat exit should not rewrite a slot'
a.uc.mem_write(0x1318F01C,pack(1))
assert a.call('OverlaySave')==1 and a.files[slot0]==saved(m,9),'second save must alternate with increasing generation'
failure_results=[]
for fault,expected in [('open',-2),('write',-3),('short',-3),('sync',-3),('close',-3),('reopen',-4),('readback',-5)]:
    a=Arm({slot0:saved(m,7)},fault);a.call('OverlayInit');a.call('OverlayInstallCode')
    a.uc.mem_write(0x1318F01C,pack(1))
    assert a.call('OverlaySave')==expected&0xffffffff,fault
    assert a.files[slot0]==saved(m,7),'previous valid slot destroyed: '+fault
    assert a.uc.mem_read(0x1318F01C,4)==pack(1),'failure cleared unsaved marker: '+fault
    failure_results.append(fault)
for files,generation in [({slot0:saved(m,7),slot1:saved(m,8)[:48]},7),
                         ({slot0:saved(m,0xffffffff),slot1:saved(m,0)},0)]:
    a=Arm(files);a.call('OverlayInit');a.call('OverlayInstallCode')
    assert a.maps()==m and a.uc.mem_read(0x1318F020,4)==pack(generation)
reports.append({'loader_heap_untouched_until_install':True,'repeat_save_alternates':True,
                'injected_FAT_failures':failure_results,'truncated_and_wrapped_generation_recovery':True})
reports.append({'ARM_runtime_file_roundtrip':'passed','file_bytes':96,'restored_generation':8,
 'dirty_false_writes':0,'invalid_word_rejected_before_write':True,'fat_backend':'in-memory stub; not physical FAT/USB'})
(base/'m21-bus-save-verification.json').write_text(json.dumps(reports,indent=2));print(json.dumps(reports,indent=2))
