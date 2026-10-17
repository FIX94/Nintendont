import os
"""Execute the final PADReadGC exit branch, DSP stop and ARM handshake.
The real hardware return stub and USB shutdown still need a Wii test.
"""
from pathlib import Path
import struct,sys,json,subprocess
base=Path(__file__).resolve().parent;# Install unicorn from requirements.txt before running.
from unicorn import *
from unicorn.ppc_const import *
src=Path(os.environ.get('NIN_SOURCE',str(base.parents[1])));binary=(src/'loader/data/PADReadGC.bin').read_bytes()
pack=lambda n:struct.pack('>I',n)
# lis r9,D318; ori r9,r9,F2C8; lwz r9,0(r9)
needle=bytes.fromhex('3d20d3186129f2c881290000')
assert binary.count(needle)==1
offset=binary.index(needle);entry=0x93000000+offset
branch=struct.unpack_from('>I',binary,offset+16)[0]
assert branch&0xFFFF0000==0x419E0000,'zero flag must branch away from exit'
delta=struct.unpack('>h',pack(branch)[2:])[0];normal=entry+16+delta
records=[]
for flag in (0,1):
    uc=Uc(UC_ARCH_PPC,UC_MODE_32|UC_MODE_BIG_ENDIAN)
    for a,n in [(0x93000000,0x10000),(0xD318F000,0x1000),(0xD3003000,0x1000),(0xCC005000,0x1000),(0x81000000,0x10000)]:uc.mem_map(a,n)
    uc.mem_write(0x93000000,binary);uc.mem_write(0xD318F2C8,pack(flag))
    uc.mem_write(0xCC005036,bytes.fromhex('FFFF'));uc.reg_write(UC_PPC_REG_1,0x8100FF00)
    writes=[]
    def record(uc,access,a,n,v,data):
        writes.append((a,n,v))
        if a==0xD3003420:uc.emu_stop()
    uc.hook_add(UC_HOOK_MEM_WRITE,record)
    uc.emu_start(entry,normal,count=1000)
    if flag:
        assert writes[-1]==(0xD3003420,4,0x1DEA),writes
        assert (0xCC005036,2,0x7FFF) in writes,'DSP DMA must stop before exit handshake'
    else:
        assert uc.reg_read(UC_PPC_REG_PC)==normal and not writes
    records.append({'exit_flag':flag,'writes':[{'address':hex(a),'bytes':n,'value':hex(v)} for a,n,v in writes]})
# Prove the C ABI gives the offset the independently compiled caller uses.
prefix=str(Path(os.environ['DEVKITPPC'])/'bin/powerpc-eabi-')
check=base/'m21-exit-abi.c'
check.write_text('#include "%s"\n_Static_assert(__builtin_offsetof(OverlayState,exitRequested)==712,"exit request offset");\n_Static_assert(sizeof(OverlayState)==868,"ABI size");\n'%(src/'common/include/Overlay.h').as_posix())
subprocess.run([prefix+'gcc'+('.exe' if os.name=='nt' else ''),'-c',str(check),'-o',str(base/'m21-exit-abi.o')],check=True)
report={'caller_exit_instruction_address':hex(entry),'C_ABI_exit_offset':712,'cases':records,'hardware_return_stub':'not emulated'}
(base/'m21-exit-verification.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
