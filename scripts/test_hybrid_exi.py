"""CPU-level hybrid EXI checks. Build first; requires Python and unicorn."""

import argparse
import itertools
import re
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

from unicorn import Uc, UcError, UC_ARCH_ARM, UC_ARCH_PPC, UC_MODE_ARM, UC_MODE_BIG_ENDIAN, UC_MODE_32, UC_HOOK_CODE, UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC
from unicorn.ppc_const import UC_PPC_REG_0, UC_PPC_REG_1, UC_PPC_REG_3, UC_PPC_REG_4, UC_PPC_REG_5, UC_PPC_REG_6, UC_PPC_REG_7, UC_PPC_REG_9, UC_PPC_REG_10, UC_PPC_REG_11, UC_PPC_REG_12, UC_PPC_REG_29, UC_PPC_REG_30, UC_PPC_REG_31, UC_PPC_REG_CR, UC_PPC_REG_XER, UC_PPC_REG_LR, UC_PPC_REG_PC


REPO = Path(__file__).resolve().parents[1]
ARM = None
GAME_IMAGE = None


def function(source, signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def words(*values):
    return struct.pack(">" + "I" * len(values), *values)


def u32(cpu, address):
    return struct.unpack(">I", cpu.mem_read(address, 4))[0]


def branch_target(op, address):
    displacement = op & 0x03FFFFFC
    if displacement & 0x02000000:
        displacement -= 0x04000000
    return (address + displacement) & 0xFFFFFFFF


class HybridEXITests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temp.cleanup)
        directory = Path(cls.temp.name)
        patch = (REPO / "kernel" / "Patch.c").read_text(encoding="latin-1")
        cls.blobs = {
            name: (REPO / "kernel" / "asm" / (name + ".bin")).read_bytes()
            for name in ("FakeInterrupt", "FakeInterrupt_DBG", "FakeInterrupt_Datel",
                         "HybridEXIGate", "HybridEXIGate_DBG")
        }
        arrays = "\n".join(
            "const u8 %s[] = {%s};\nconst u32 %s_size = %u;" %
            (name, ",".join(map(str, data)), name, len(data))
            for name, data in cls.blobs.items()
        )
        harness = r"""
#include "CommonConfig.h"
typedef unsigned int u32;
typedef int s32;
typedef short s16;
typedef unsigned char u8;
typedef volatile unsigned int vu32;
typedef int bool;
#define true 1
static u32 POffset;
u32 DisableEXIPatch, DisableSIPatch;
static int active;
static inline u32 read32(u32 p) { return *(vu32*)p; }
static inline void write32(u32 p, u32 v) { *(vu32*)p = v; }
int EXIHybridEnabled(void) { return active; }
int dbgprintf(const char *s, ...) { return 0; }
void Shutdown(void) { __asm__ volatile("bkpt 0"); for (;;) {} }
void *memcpy(void *d, const void *s, unsigned int n) {
    u8 *out = d; const u8 *in = s;
    while (n--) *out++ = *in++;
    return d;
}
void reset(u32 enabled, u32 offset) {
    active = enabled; POffset = offset;
}
void interface_mode(u32 native) { DisableEXIPatch = native; }
int gate(u32 config, u32 wiiu, u32 triforce) {
    return NIN_CFG_HYBRID_EXI_ACTIVE(config, wiiu, triforce);
}
"""
        harness += arrays + "\n"
        for signature in ("void PatchB(", "void PatchBL(", "u32 PatchCopy(",
                          "static void PatchHybridEXI(",
                          "static void PatchHybridEXIMask(",
                          "static void PatchHybridEXIDispatch(",
                          "void PatchFuncInterface("):
            harness += function(patch, signature) + "\n"
        loader = (REPO / "loader" / "source" / "global.c").read_text(encoding="latin-1")
        harness += """
static NIN_CFG cfg_storage;
static NIN_CFG *ncfg = &cfg_storage;
""" + function(loader, "void UpdateNinCFG(") + "\n"
        harness += """
u32 upgrade_config(u32 version, u32 config, u32 slot) {
    cfg_storage.Version = version; cfg_storage.Config = config;
    cfg_storage.WiiUGamepadSlot = slot;
    UpdateNinCFG();
    return cfg_storage.Config;
}
u32 config_version(void) { return cfg_storage.Version; }
u32 config_slot(void) { return cfg_storage.WiiUGamepadSlot; }
u32 current_config_version(void) { return NIN_CFG_VERSION; }
void route(u32 address, const u8 *original, u32 length, u32 size) {
    PatchHybridEXI(address, original, length, size);
}
void verify_dispatch(u32 address, u32 debug) {
    PatchHybridEXIDispatch(address, debug);
}
u32 patch_dispatcher(u32 address) {
    u32 fake = PatchCopy(FakeInterrupt, FakeInterrupt_size);
    PatchBL(fake, address + 4);
    PatchHybridEXIDispatch(address, 0);
    return fake;
}
void mask_hook(u32 address) { PatchHybridEXIMask(address); }
u32 patch_offset(void) { return POffset; }
"""
        source = directory / "harness.c"
        elf = directory / "harness.elf"
        source.write_text(harness, encoding="ascii")
        subprocess.run([
            str(ARM / "bin" / "arm-none-eabi-gcc.exe"), "-mbig-endian", "-marm",
            "-mcpu=arm926ej-s", "-O2", "-fno-builtin", "-nostdlib",
            "-I" + str(REPO / "common" / "include"), str(source),
            "-Wl,-Ttext=0x500000,-Tdata=0x510000,-e,route", "-o", str(elf)
        ], check=True)
        symbols = subprocess.check_output([
            str(ARM / "bin" / "arm-none-eabi-nm.exe"), str(elf)
        ], text=True)
        cls.symbols = {}
        for line in symbols.splitlines():
            fields = line.split()
            if len(fields) == 3:
                cls.symbols[fields[2]] = int(fields[0], 16)
        cls.image = elf.read_bytes()

    def setUp(self):
        self.arm = Uc(UC_ARCH_ARM, UC_MODE_ARM | UC_MODE_BIG_ENDIAN)
        self.arm.mem_map(0, 0x400000)
        self.arm.mem_map(0x4F0000, 0x30000)
        self.arm.mem_map(0x13020000, 0x10000)
        self.arm.mem_map(0x0D800000, 0x10000)
        header = struct.unpack_from(">16sHHIIIIIHHHHHH", self.image)
        for index in range(header[10]):
            segment = struct.unpack_from(">IIIIIIII", self.image, header[5] + index * header[9])
            kind, offset, address, _, size, _, _, _ = segment
            if kind == 1:
                self.arm.mem_write(address, self.image[offset:offset + size])
        self.call("reset", 1, 0x2F00)

    def call(self, name, *args):
        for reg, value in zip((UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3), args):
            self.arm.reg_write(reg, value)
        self.arm.reg_write(UC_ARM_REG_SP, 0x3FF000)
        self.arm.reg_write(UC_ARM_REG_LR, 0x3F0000)
        self.arm.emu_start(self.symbols[name], 0x3F0000, count=100000)
        self.assertEqual(self.arm.reg_read(UC_ARM_REG_PC), 0x3F0000)
        return self.arm.reg_read(UC_ARM_REG_R0)

    def test_activation_truth_table(self):
        for memcard, hybrid, wiiu, triforce in itertools.product((0, 1), repeat=4):
            config = (memcard << 3) | (hybrid << 20) | (1 << 19)
            with self.subTest(memcard=memcard, hybrid=hybrid, wiiu=wiiu, triforce=triforce):
                self.assertEqual(self.call("gate", config, wiiu, triforce),
                                 int(bool(memcard and hybrid and not wiiu and not triforce)))

    def test_config_upgrade_defaults_hybrid_exi_off(self):
        hybrid, others = 1 << 20, (1 << 3) | (1 << 19)  # Memcard Emulation, BBA Emulation
        self.assertEqual(self.call("current_config_version"), 11)
        # A stale bit 20 in an older nincfg.bin or loader argument is cleared once.
        for version in (9, 10):
            with self.subTest(version=version):
                self.assertEqual(self.call("upgrade_config", version, others | hybrid, 0), others)
                self.assertEqual(self.call("config_version"), 11)
        # The user's saved choice persists in a current config.
        for config in (others, others | hybrid):
            with self.subTest(config=hex(config)):
                self.assertEqual(self.call("upgrade_config", 11, config, 0), config)
                self.assertEqual(self.call("config_version"), 11)
        for version in (10, 11):
            with self.subTest(version=version, slot="garbage"):
                self.call("upgrade_config", version, 0, 99)
                self.assertEqual(self.call("config_slot"), 0)

    def test_menu_label_and_experimental_description(self):
        menu = (REPO / "loader" / "source" / "menu.c").read_text()
        label = menu.index('"%-18s:%-4s", "hybrid EXI"')
        self.assertTrue(menu[menu.rfind("PrintFormat(", 0, label):label].startswith(
            "PrintFormat(MENU_SIZE,"))
        start = menu.index("static const char *desc_hybrid_exi[]")
        description = menu[start:menu.index("};", start)]
        self.assertEqual(re.findall(r'"([^"]*)"', description)[-1], "(Experimental)")
        self.assertNotIn("hybrid EXI (exp)", menu)

    def test_native_entry_and_emulated_routing(self):
        for prologue in (0x7C0802A6, 0x9421FFF0):  # mflr r0; stwu r1,-16(r1)
            self.call("reset", 1, 0x2F00)
            address, backup = 0xC5088, 0x200000
            native = words(prologue, 0x39201111,
                           0x38210010 if prologue == 0x9421FFF0 else 0x60000000,
                           0x4E800020)
            self.arm.mem_write(backup, native)
            self.arm.mem_write(address, words(0x39202222, 0x4E800020))
            self.call("route", address, backup, len(native), 8)
            self.assertEqual(bytes(self.arm.mem_read(address + 4, len(native) - 4)), native[4:])
            for channel in (0, 1, 2):
                cpu = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
                cpu.mem_map(0, 0x100000)
                cpu.mem_map(0xD3020000, 0x10000)
                cpu.mem_write(0, bytes(self.arm.mem_read(0, 0x100000)))
                cpu.reg_write(UC_PPC_REG_1, 0xF0000)
                cpu.reg_write(UC_PPC_REG_3, channel)
                for reg in (UC_PPC_REG_4, UC_PPC_REG_5, UC_PPC_REG_6, UC_PPC_REG_7,
                            UC_PPC_REG_11, UC_PPC_REG_12):
                    cpu.reg_write(reg, 0x76543210)
                cpu.reg_write(UC_PPC_REG_LR, 0xF8000)
                writes = []
                cpu.hook_add(UC_HOOK_MEM_WRITE,
                             lambda uc, access, address, size, value, data:
                             writes.append(address))
                cpu.emu_start(address, 0xF8000, count=200)
                with self.subTest(prologue=hex(prologue), channel=channel):
                    self.assertEqual(cpu.reg_read(UC_PPC_REG_PC), 0xF8000)
                    self.assertEqual(cpu.reg_read(UC_PPC_REG_9), 0x1111 if channel == 1 else 0x2222)
                    self.assertEqual(cpu.reg_read(UC_PPC_REG_1), 0xF0000)
                    self.assertEqual(cpu.reg_read(UC_PPC_REG_LR), 0xF8000)
                    for reg in (UC_PPC_REG_4, UC_PPC_REG_5, UC_PPC_REG_6, UC_PPC_REG_7,
                                UC_PPC_REG_11, UC_PPC_REG_12):
                        self.assertEqual(cpu.reg_read(reg), 0x76543210)
                    # The wrapper itself owns no stack frame and records nothing.
                    self.assertEqual(writes, [0xF0000 - 16] if channel == 1 and
                                     prologue == 0x9421FFF0 else [])

    def test_dispatch_patches_both_mmio_aliases_and_rejects_mismatch(self):
        address = 0x200000
        gates = {0: self.blobs["HybridEXIGate"], 1: self.blobs["HybridEXIGate_DBG"]}
        for debug, alias, gate_site, channel2 in itertools.product(
                (0, 1), (0xCC00, 0xCD80), (0, 1), (0, 1)):
            self.call("reset", 1, 0x2F00)
            self.arm.mem_write(address - 4, bytes(0x204))
            slot_a = address + (0x138 if debug else 0x114)
            self.arm.mem_write(slot_a, words(0x3C600000 | alias,
                                            0x83C36800 if debug else 0x80836800))
            if not debug:
                self.arm.mem_write(address + 0x14C, words(0x3C600000 | alias,
                                                          0x38636800, 0x80830014))
                if gate_site:
                    self.arm.mem_write(slot_a + 8, words(0x548307BC, 0x28030000))
                if channel2:
                    self.arm.mem_write(address + 0x188, words(0x3C600000 | alias,
                                                              0x38636800, 0x80830028))
            before = bytes(self.arm.mem_read(address - 4, 0x204))
            self.call("verify_dispatch", address, debug)
            with self.subTest(debug=debug, alias=hex(alias), gate_site=gate_site,
                              channel2=channel2):
                op = u32(self.arm, slot_a)
                if debug or gate_site:
                    self.assertEqual(op & 0xFC000003, 0x48000001)
                    gate = branch_target(op, slot_a)
                    self.assertEqual(bytes(self.arm.mem_read(gate, len(gates[debug]))),
                                     gates[debug])
                    self.assertEqual(u32(self.arm, slot_a + 4), 0x60000000)
                else:
                    self.assertEqual(op, 0x3C60C000)
                    self.assertEqual(u32(self.arm, slot_a + 4), 0x80830010)
                # Only the Slot A, Slot B and EXI 2 status loads are changed.
                expected = bytearray(before)
                expected[slot_a - address + 4:slot_a - address + 12] = \
                    self.arm.mem_read(slot_a, 8)
                if not debug:
                    self.assertEqual(u32(self.arm, address + 0x14C), 0x3C60CD80)
                    self.assertEqual(u32(self.arm, address + 0x190),
                                     0x38800000 if channel2 else 0)
                    for offset in (0x14C, 0x190):
                        expected[offset + 4:offset + 8] = self.arm.mem_read(address + offset, 4)
                self.assertEqual(bytes(self.arm.mem_read(address - 4, 0x204)), bytes(expected))
        self.arm.mem_write(address + 0x114, words(0x60000000, 0x80836800))
        with self.assertRaises(UcError):
            self.call("verify_dispatch", address, 0)

    def run_gate(self, name, exi_int, word):
        cpu = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
        for base, size in ((0, 0x100000), (0xC0000000, 0x10000), (0xD3020000, 0x10000)):
            cpu.mem_map(base, size)
        cpu.mem_write(0x1000, self.blobs[name])
        cpu.mem_write(0xC0000010, words(word))
        cpu.mem_write(0xD3026560, words(exi_int))
        for reg in range(32):
            if reg not in (1, 3):
                cpu.reg_write(UC_PPC_REG_0 + reg, 0x1000 * reg + 0x55)
        cpu.reg_write(UC_PPC_REG_1, 0xF0000)
        cpu.reg_write(UC_PPC_REG_CR, 0x24000000)
        cpu.reg_write(UC_PPC_REG_XER, 0x20000000)
        cpu.reg_write(UC_PPC_REG_LR, 0xF8000)
        writes = []
        cpu.hook_add(UC_HOOK_MEM_WRITE,
                     lambda uc, access, address, size, value, data: writes.append(address))
        cpu.emu_start(0x1000, 0xF8000, count=100)
        self.assertEqual(cpu.reg_read(UC_PPC_REG_PC), 0xF8000)
        self.assertEqual(cpu.reg_read(UC_PPC_REG_3), 0xC0000000)
        self.assertEqual(cpu.reg_read(UC_PPC_REG_1), 0xF0000)
        self.assertEqual(writes, [])
        return cpu

    def test_slot_a_gate_drops_stale_virtual_status(self):
        for exi_int, word in itertools.product((0, 0x10), (0, 2)):
            pending = bool(exi_int)
            with self.subTest(gate="normal", exi_int=exi_int, word=word):
                cpu = self.run_gate("HybridEXIGate", exi_int, word)
                self.assertEqual(cpu.reg_read(UC_PPC_REG_4), word if pending else 0)
                # r0 accumulates the interrupt cause in __OSDispatchInterrupt.
                for reg in [0] + list(range(5, 32)):
                    self.assertEqual(cpu.reg_read(UC_PPC_REG_0 + reg), 0x1000 * reg + 0x55)
                self.assertEqual(cpu.reg_read(UC_PPC_REG_XER), 0x20000000)
            with self.subTest(gate="debug", exi_int=exi_int, word=word):
                cpu = self.run_gate("HybridEXIGate_DBG", exi_int, word)
                self.assertEqual(cpu.reg_read(UC_PPC_REG_30), word if pending else 0)
                self.assertEqual(cpu.reg_read(UC_PPC_REG_CR), 0x24000000)
                self.assertEqual(cpu.reg_read(UC_PPC_REG_XER), 0x20000000)
                for reg in range(32):
                    if reg not in (1, 3, 30):
                        self.assertEqual(cpu.reg_read(UC_PPC_REG_0 + reg), 0x1000 * reg + 0x55)

    def test_interrupt_mask_hook_skips_branch_prologue(self):
        self.arm.mem_write(0x200000, words(0x48000010, 0x60000000))
        self.call("mask_hook", 0x200000)
        self.assertEqual(u32(self.arm, 0x200000), 0x48000010)
        self.assertEqual(self.call("patch_offset"), 0x2F00)

    def test_native_mmio_remapping(self):
        for hybrid, native in ((0, 0), (0, 1), (1, 0)):
            self.call("reset", hybrid, 0x2F00)
            self.call("interface_mode", native)
            for instruction in (0x38636800, 0x60636800, 0x80836814):
                self.arm.mem_write(0x200000, words(0x3C60CC00, instruction, 0x4E800020))
                self.call("PatchFuncInterface", 0x200000, 12)
                with self.subTest(hybrid=hybrid, native=native, instruction=hex(instruction)):
                    self.assertEqual(u32(self.arm, 0x200000),
                                     0x3C60CD80 if hybrid or native else 0x3C60CC00)

    def test_emulated_routines_have_only_internal_relative_branches(self):
        for name in ("EXIImm", "EXISelect", "EXILock", "EXIDMA", "EXIProbe"):
            blob = (REPO / "kernel" / "asm" / (name + ".bin")).read_bytes()
            for offset in range(0, len(blob), 4):
                op = struct.unpack_from(">I", blob, offset)[0]
                if (op >> 26) not in (16, 18) or (op & 2):
                    continue
                bits = 16 if (op >> 26) == 16 else 26
                displacement = op & ((1 << bits) - 4)
                if displacement & (1 << (bits - 1)):
                    displacement -= 1 << bits
                with self.subTest(routine=name, offset=offset):
                    self.assertTrue(0 <= offset + displacement < len(blob))

    def test_unsupported_prologue_and_patch_exhaustion_stop(self):
        self.arm.mem_write(0x200000, words(0x48000004))
        with self.assertRaises(UcError):
            self.call("route", 0xC5088, 0x200000, 4, 4)
        self.call("reset", 1, 0x1800)
        with self.assertRaises(UcError):
            self.call("PatchCopy", self.symbols["FakeInterrupt"], len(self.blobs["FakeInterrupt"]))

    def test_irq_merge_and_legacy_identity(self):
        for name, blob in self.blobs.items():
            if not name.startswith("FakeInterrupt"):
                continue
            self.call("reset", 0, 0x2F00)
            address = self.call("PatchCopy", self.symbols[name], len(blob))
            self.assertEqual(bytes(self.arm.mem_read(address, len(blob))), blob)
            self.call("reset", 1, 0x2F00)
            address = self.call("PatchCopy", self.symbols[name], len(blob))
            patched = bytes(self.arm.mem_read(address, len(blob)))
            self.assertNotEqual(patched, blob)
            for physical, synthetic in itertools.product((0, 0x10), repeat=2):
                cpu = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
                for base, size in ((0, 0x100000), (0xCC000000, 0x10000),
                                   (0xCD800000, 0x10000), (0xD3020000, 0x10000)):
                    cpu.mem_map(base, size)
                cpu.mem_write(address, patched)
                cpu.mem_write(0xCC003000, words(0x4000 | physical))
                cpu.mem_write(0xD3026560, words(synthetic))
                cpu.reg_write(UC_PPC_REG_3, 0xCC000000)
                cpu.reg_write(UC_PPC_REG_LR, 0xF8000)
                cpu.emu_start(address, 0xF8000, count=100)
                self.assertEqual(cpu.reg_read(UC_PPC_REG_PC), 0xF8000)
                result = cpu.reg_read(UC_PPC_REG_29 if name.endswith("_DBG") else UC_PPC_REG_31)
                if name.endswith("_Datel"):
                    result = cpu.reg_read(UC_PPC_REG_10)
                with self.subTest(handler=name, physical=physical, synthetic=synthetic):
                    self.assertEqual(result & 0x10, physical | synthetic)

    def test_physical_irq_does_not_clear_virtual_irq(self):
        blob = (REPO / "kernel" / "asm" / "TCIntrruptHandlerHybrid.bin").read_bytes()
        for irq in range(9, 17):
            cpu = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
            cpu.mem_map(0, 0x100000)
            cpu.mem_map(0xD3020000, 0x10000)
            cpu.mem_write(0x2000, blob)
            cpu.mem_write(0xD3026560, words(0x10))
            cpu.reg_write(UC_PPC_REG_0, 0x12345678)
            cpu.reg_write(UC_PPC_REG_1, 0xF0000)
            cpu.reg_write(UC_PPC_REG_3, irq)
            cpu.reg_write(UC_PPC_REG_LR, 0xF8000)
            writes = []
            cpu.hook_add(UC_HOOK_MEM_WRITE,
                         lambda uc, access, address, size, value, data: writes.append(address))
            cpu.emu_start(0x2000, 0xF8000, count=30)
            physical = 12 <= irq <= 14
            with self.subTest(irq=irq):
                self.assertEqual(cpu.reg_read(UC_PPC_REG_PC), 0xF8000)
                self.assertEqual(u32(cpu, 0xD3026560), 0x10 if physical else 0)
                self.assertEqual(u32(cpu, 0xF0004), 0x12345678)
                self.assertEqual(writes, [0xF0004] if physical else [0xF0004, 0xD3026560])

    def load_local_game(self):
        if GAME_IMAGE is None:
            self.skipTest("optional --game-image not supplied")
        if not GAME_IMAGE.is_file():
            self.skipTest(f"optional game image not found: {GAME_IMAGE}")
        with GAME_IMAGE.open("rb") as disc:
            if disc.read(6) != b"GP7E01":
                self.skipTest(f"optional game image is not GP7E01: {GAME_IMAGE}")
            disc.seek(0x420)
            dol_offset = struct.unpack(">I", disc.read(4))[0]
            disc.seek(dol_offset)
            header = disc.read(0x100)
            for index in range(18):
                offset, address, size = [
                    struct.unpack_from(">I", header, base + index * 4)[0]
                    for base in (0, 0x48, 0x90)
                ]
                if size:
                    disc.seek(dol_offset + offset)
                    self.arm.mem_write(address & 0x7FFFFFFF, disc.read(size))

    def test_local_gp7e01_native_irq_callbacks(self):
        self.load_local_game()
        self.call("interface_mode", 0)
        self.call("PatchFuncInterface", 0xC5E2C, 0x2DC)
        helper = (REPO / "kernel" / "asm" / "TCIntrruptHandlerHybrid.bin").read_bytes()
        self.arm.mem_write(0x300000, helper)
        hook = self.call("PatchCopy", 0x300000, len(helper))
        for address in (0xC5E2C, 0xC5EF4):
            self.call("PatchBL", hook, address + 4)
        for irq in (9, 10, 12, 13):
            with self.subTest(irq=irq):
                cpu = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
                cpu.mem_map(0x80000000, 0x2000000)
                cpu.mem_map(0xCD800000, 0x10000)
                cpu.mem_map(0xD3020000, 0x10000)
                cpu.mem_write(0x80000000, bytes(self.arm.mem_read(0, 0x400000)))
                for stub in (0x800A0310, 0x800A0148, 0x800A1D20):
                    cpu.mem_write(stub, words(0x4E800020))
                # Model callback work only; execute the actual native SDK handlers.
                cpu.mem_write(0x81000100, words(0x3CA08100, 0x90650200, 0x90850204, 0x4E800020))
                channel = int(irq >= 12)
                tc = irq in (10, 13)
                state = 0x802BA6F0 + channel * 0x40
                cpu.mem_write(state + (4 if tc else 0), words(0x81000100))
                cpu.mem_write(0xD3026560, words(0x10))
                for reg, value in ((UC_PPC_REG_1, 0x81700000), (UC_PPC_REG_3, irq),
                                   (UC_PPC_REG_4, 0x81600000),
                                   (UC_PPC_REG_LR, 0x81800000)):
                    cpu.reg_write(reg, value)
                address = 0x800C5EF4 if tc else 0x800C5E2C
                cpu.emu_start(address, 0x81800000, count=1000)
                self.assertEqual(cpu.reg_read(UC_PPC_REG_PC), 0x81800000)
                self.assertEqual(cpu.reg_read(UC_PPC_REG_1), 0x81700000)
                self.assertEqual(u32(cpu, 0x81000200), channel)
                self.assertEqual(u32(cpu, 0x81000204), 0x81600000)
                self.assertEqual(u32(cpu, 0xD3026560), 0x10 if channel else 0)
                self.assertEqual(bytes(cpu.mem_read(0xD3026600, 0x200)), bytes(0x200))

    def test_local_gp7e01_interrupt_mask_keeps_emulated_channels_closed(self):
        self.load_local_game()
        self.call("interface_mode", 0)
        self.call("PatchFuncInterface", 0xA1A48, 0x2D8)
        for hooked in (0, 1):
            if hooked:
                self.call("mask_hook", 0xA1A48)
            cpu = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
            cpu.mem_map(0x80000000, 0x2000000)
            cpu.mem_map(0xCC000000, 0x10000)
            cpu.mem_map(0xCD800000, 0x10000)
            cpu.mem_write(0x80000000, bytes(self.arm.mem_read(0, 0x400000)))
            # Unmask EXI 0, 1 and 2 the way __OSSetInterruptMask loops over groups.
            remaining = 0x7F8000
            for _ in range(3):
                for reg, value in ((UC_PPC_REG_1, 0x81700000), (UC_PPC_REG_3, remaining),
                                   (UC_PPC_REG_4, 0), (UC_PPC_REG_LR, 0x81800000)):
                    cpu.reg_write(reg, value)
                cpu.emu_start(0x800A1A48, 0x81800000, count=200)
                self.assertEqual(cpu.reg_read(UC_PPC_REG_PC), 0x81800000)
                remaining = cpu.reg_read(UC_PPC_REG_3)
            csr = [u32(cpu, 0xCC006800 + n * 0x14) | u32(cpu, 0xCD806800 + n * 0x14)
                   for n in range(3)]
            with self.subTest(hooked=hooked):
                self.assertEqual(remaining, 0)
                self.assertEqual(csr, [0 if hooked else 0x405, 0x405, 0 if hooked else 0x5])

    def run_gp7_dispatch(self, cause, exi_int, word, csr1, csr2):
        cpu = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
        cpu.mem_map(0x80000000, 0x2000000)
        for base in (0xC0000000, 0xCC000000, 0xCD800000, 0xD3020000):
            cpu.mem_map(base, 0x10000)
        cpu.mem_write(0x80000000, bytes(self.arm.mem_read(0, 0x400000)))
        # Stop at the handler lookup, or where no interrupt is dispatched.
        for stop in (0x800A1E78, 0x800A20F4, 0x800A2150):
            cpu.hook_add(UC_HOOK_CODE, lambda uc, address, size, data: uc.emu_stop(),
                         begin=stop, end=stop)
        cpu.mem_write(0xCC003000, words(cause, 0xFFFFFFFF))
        cpu.mem_write(0xD3026560, words(exi_int))
        cpu.mem_write(0xC0000010, words(word))
        cpu.mem_write(0xCD806814, words(csr1))
        cpu.mem_write(0xCD806828, words(csr2))
        for reg, value in ((UC_PPC_REG_1, 0x81700000), (UC_PPC_REG_4, 0x81600000),
                           (UC_PPC_REG_LR, 0x81800000)):
            cpu.reg_write(reg, value)
        cpu.emu_start(0x800A1E30, 0x81800000, count=2000)
        return cpu

    def test_local_gp7e01_dispatch_ignores_stale_and_emulated_status(self):
        self.load_local_game()
        self.call("interface_mode", 0)
        self.call("PatchFuncInterface", 0xA1E30, 0x260)
        self.call("patch_dispatcher", 0xA1E4C)
        cases = (
            # PI cause, EXI_INT, Slot A word, Slot B CSR, EXI 2 CSR, dispatched IRQ
            (0x10, 0, 2, 2, 0, 12),      # stale Slot A status on a real Slot B IRQ
            (0x4000, 0x10, 2, 0, 0, 9),  # virtual Slot A IRQ
            (0x4010, 0x10, 2, 2, 0, 9),  # both pending: Slot A keeps priority
            (0x10, 0, 0, 0, 2, None),    # EXI 2 hardware status is never dispatched
        )
        for cause, exi_int, word, csr1, csr2, expected in cases:
            cpu = self.run_gp7_dispatch(cause, exi_int, word, csr1, csr2)
            with self.subTest(cause=hex(cause), exi_int=exi_int, csr1=csr1, csr2=csr2):
                if expected is None:
                    self.assertEqual(cpu.reg_read(UC_PPC_REG_PC), 0x800A2150)
                else:
                    self.assertEqual(cpu.reg_read(UC_PPC_REG_PC), 0x800A20F4)
                    self.assertEqual(cpu.reg_read(UC_PPC_REG_29), expected)
                self.assertEqual(cpu.reg_read(UC_PPC_REG_30), 0x81600000)
                self.assertEqual(bytes(cpu.mem_read(0xD3026600, 0x200)), bytes(0x200))

    def test_local_gp7e01_dispatch_and_native_immediate_transfer(self):
        self.load_local_game()
        self.call("interface_mode", 0)
        self.call("PatchFuncInterface", 0xA1E30, 0x260)
        self.call("PatchFuncInterface", 0xC5088, 0x258)
        self.call("patch_dispatcher", 0xA1E4C)
        address, backup = 0xC5088, 0x300000
        original = bytes(self.arm.mem_read(address, 0x258))
        self.arm.mem_write(backup, original)
        blob = (REPO / "kernel" / "asm" / "EXIImm.bin").read_bytes()
        self.arm.mem_write(address, blob)
        self.call("route", address, backup, len(original), len(blob))
        cpu = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
        cpu.mem_map(0x80000000, 0x2000000)
        cpu.mem_map(0xCD800000, 0x10000)
        cpu.mem_map(0xD3020000, 0x10000)
        cpu.mem_write(0x80000000, bytes(self.arm.mem_read(0, 0x400000)))
        # Model only OS interrupt enable/disable; execute the actual native EXIImm.
        for stub in (0x800A1958, 0x800A1980):
            cpu.mem_write(stub, words(0x38600001, 0x4E800020))
        cpu.mem_write(0x802BA730 + 12, words(4))  # SDK channel 1 selected.
        cpu.mem_write(0x81000000, words(0xAABBCCDD))
        for reg, value in ((UC_PPC_REG_1, 0x81700000), (UC_PPC_REG_3, 1),
                           (UC_PPC_REG_4, 0x81000000), (UC_PPC_REG_5, 4),
                           (UC_PPC_REG_6, 1), (UC_PPC_REG_7, 0),
                           (UC_PPC_REG_LR, 0x81800000)):
            cpu.reg_write(reg, value)
        cpu.emu_start(0x80000000 + address, 0x81800000, count=1000)
        self.assertEqual(cpu.reg_read(UC_PPC_REG_PC), 0x81800000)
        self.assertEqual(cpu.reg_read(UC_PPC_REG_3), 1)
        self.assertEqual(u32(cpu, 0xCD806824), 0xAABBCCDD)
        self.assertEqual(u32(cpu, 0xCD806820), 0x35)
        self.assertEqual(bytes(cpu.mem_read(0xD3026600, 0x200)), bytes(0x200))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--devkitarm", required=True, type=Path)
    parser.add_argument("--game-image", type=Path, help="Optional local GP7E01 ISO; no game data is saved.")
    args, remaining = parser.parse_known_args()
    ARM = args.devkitarm
    GAME_IMAGE = args.game_image
    unittest.main(argv=[__file__] + remaining)
