import os
"""Check the actual embedded raw ARM kernel, not just the overlay module."""
from pathlib import Path
import sys, struct, subprocess, zipfile, io, json, hashlib
base = Path(__file__).resolve().parent
# Install unicorn from requirements.txt before running.
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_MODE_BIG_ENDIAN
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_PC, UC_ARM_REG_R1
sdk = Path(os.environ['DEVKITPRO'])
arm = sdk / 'devkitARM/bin/arm-none-eabi-'
ppc = sdk / 'devkitPPC/bin/powerpc-eabi-'
def run(prefix, name, *args):
    return subprocess.check_output([str(prefix)+name+('.exe' if os.name=='nt' else ''), *map(str,args)], text=True)
def symbols(prefix, elf):
    return {p[2]:int(p[0],16) for line in run(prefix,'nm',elf).splitlines()
            if len(p:=line.split())==3 and p[1].lower()!='u'}
def dol_bytes(dol, addr, size):
    words=struct.unpack('>54I',dol[:216])
    for off,va,n in zip(words[:18],words[18:36],words[36:54]):
        if va<=addr and addr+size<=va+n:
            return dol[off+addr-va:off+addr-va+size]
    raise AssertionError('DOL symbol outside loaded sections')
reports=[]
for version in ('m21',):
    src=Path(os.environ.get('NIN_SOURCE',str(base.parents[1])))
    ks=symbols(arm,src/'kernel/kernel.elf')
    ls=symbols(ppc,src/'loader/loader.elf')
    dol=(src/'loader/loader.dol').read_bytes()
    size=struct.unpack('>I',dol_bytes(dol,ls['kernel_zip_size'],4))[0]
    compressed=dol_bytes(dol,ls['kernel_zip'],size)
    with zipfile.ZipFile(io.BytesIO(compressed)) as z:
        assert z.testzip() is None
        kernel=z.read('kernel.bin')
    assert kernel==(src/'kernel/kernel.bin').read_bytes()
    elf=(src/'kernel/kernel.elf').read_bytes()
    elf_entry=struct.unpack_from('>I',elf,24)[0]
    assert elf_entry==ks['_start']
    record={'build':version,'raw_entry':'0x12F00000','start':hex(ks['_start']),
            'elf_entry':hex(elf_entry),'entry_valid':ks['_start']==0x12F00000,
            'embedded_kernel_sha256':hashlib.sha256(kernel).hexdigest()}
    if version=='m15':
        assert ks['OverlayInit']==0x12F00000 and not record['entry_valid']
        record['failure']='Raw boot jumps into OverlayInit and skips _start/_main initialization'
    else:
        assert record['entry_valid']
        uc=Uc(UC_ARCH_ARM,UC_MODE_ARM|UC_MODE_BIG_ENDIAN)
        uc.mem_map(0x12F00000,0x100000)
        uc.mem_write(0x12F00000,kernel)
        uc.reg_write(UC_ARM_REG_SP,0x12FA8000) # stack passed by kernelboot
        uc.reg_write(UC_ARM_REG_R1,0xDEADBEEF)
        uc.emu_start(0x12F00000,ks['_main'],count=16)
        assert uc.reg_read(UC_ARM_REG_PC)==ks['_main']
        assert uc.reg_read(UC_ARM_REG_SP)==ks['__phy_stack_addr']==0x12FE2000
        assert uc.reg_read(UC_ARM_REG_R1)==0
        record['bootstrap']='Actual ARM instructions reach _main with correct stack and arguments'
    reports.append(record)
# Prove a misplaced raw entry causes a LINK ERROR, not a silently bad DOL.
tmp=base/'kernel-entry-regression';tmp.mkdir(exist_ok=True)
(tmp/'bad.s').write_text('.arm\n.text\n.global _start\ndummy: nop\n_start: b _start\n')
run(arm,'as','-mbig-endian','-mcpu=arm926ej-s','-o',tmp/'bad.o',tmp/'bad.s')
r=subprocess.run([str(arm)+'ld'+('.exe' if os.name=='nt' else ''),'-EB','-T',str(Path(os.environ.get('NIN_SOURCE',str(base.parents[1])))/'kernel/kernel.ld'),
                  '-o',str(tmp/'bad.elf'),str(tmp/'bad.o')],capture_output=True,text=True)
assert r.returncode!=0 and 'Nintendont raw kernel must start' in r.stderr,r.stderr
report={'builds':reports,'negative_link_test':r.stderr.strip()}
(base/'m21-kernel-entry-verification.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
