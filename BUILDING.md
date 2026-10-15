# Building Nintendont

```bash
scripts/setup-toolchain.sh   # once
scripts/build.sh             # ~1 minute from clean
```

That produces `loader/loader.dol` (rename to `boot.dol`) and `nintendont/boot.dol`.

Works on Windows (MSYS2 / Git Bash), Linux and macOS. On Windows, run it from
devkitPro's own MSYS2 shell (Start Menu → devkitPro → MSys2) or Git Bash.

---

## What you are building

Nintendont is **two programs for two different processors**, which is why two
compilers are needed:

| Part | Runs on | Compiler | Source |
|---|---|---|---|
| Loader | PowerPC (Broadway) — the Wii's main CPU | devkitPPC | `loader/` |
| Kernel | ARM (Starlet) — the Wii's I/O coprocessor, inside IOS | devkitARM | `kernel/` |

The loader draws the menu, picks the game, and boots the kernel into IOS. The
kernel then emulates GameCube hardware (disc, memory cards, controllers, BBA)
for the running game. The built kernel is compressed into
`loader/data/kernel.zip` and embedded in the loader, so **the kernel is built
first** and the loader carries it.

You are cross-compiling: an x86 PC producing big-endian PowerPC and ARM code
that never runs on the build machine. Nothing here can be tested by running it
locally, and Dolphin cannot run Nintendont either, because Dolphin does not
emulate the ARM side.

Which subproject needs which compiler, since it is not obvious from the layout:

- **devkitARM** — `kernel`, `kernelboot`, `fatfs` (ARM variant)
- **devkitPPC** — `loader`, `multidol`, `resetstub`, `codehandler`, `kernel/asm`,
  `fatfs` (PPC variant), `loader/source/ppc/{PADReadGC,IOSInterface}`

`kernel/asm` is PowerPC despite living under `kernel/`: it assembles PPC patch
code that the ARM kernel injects into the running game.

---

## The pinned toolchain

`scripts/setup-toolchain.sh` installs these **side by side** with any devkitPro
you already have, so it will not disturb other projects:

| Component | Version | Installed as |
|---|---|---|
| devkitARM | r53-1 (gcc 9.1.0) | `$DEVKITPRO/devkitARM-r53-1` |
| devkitPPC | r35-2 (gcc 8.3.0) | `$DEVKITPRO/devkitPPC-r35-2` |
| libOGC | 1.8.23-1 | `$DEVKITPRO/libogc-1.8.23` |

These are not arbitrary. Nintendont has not been updated for the modern
toolchain, and each mismatch fails in a way that looks like broken source:

- **libOGC 1.8.23 requires devkitPPC r35's C runtime.** Against r41+ the link
  ends with `undefined reference to __syscalls`.
- **Current libogc removed the internals Nintendont calls** —
  `__lwp_thread_stopmultitasking` and `__lwp_thread_closeall`. Extrems' libogc2
  fork has those but removed `raw_irq_handler_t`, which Nintendont also uses.
  Neither can build it; these are API removals, so no include path fixes them.
- **devkitPPC r41+ dropped the `bin2o` make macro** that `loader/Makefile` uses
  to embed `background.png`, `font.zip` and `kernel.zip`. Without it the recipe
  expands to nothing and the build fails much later.
- **gcc 14+ turns `-Wincompatible-pointer-types` into an error**, which stops
  `multidol/apploader.c` — a warning on the pinned gcc 8.3.

devkitPro's servers only ever serve the *current* release, by policy, so these
versions come from the community archive mirror `wii.leseratte10.de`. The
`README.md` also links a Mediafire folder; the two were verified byte-identical
(SHA-256 in `scripts/setup-toolchain.sh`, which refuses to install on mismatch).

### Using a different toolchain on purpose

```bash
NINTENDONT_DEVKITPPC=/opt/devkitpro/devkitPPC scripts/build.sh
```

The pinned paths deliberately override anything inherited from the environment,
because devkitPro's installer exports `DEVKITPPC`/`DEVKITARM` globally and a
silently-inherited value is a very confusing way to build against the wrong
compiler.

---

## Build targets

```bash
scripts/build.sh              # everything (default)
scripts/build.sh kernel       # ARM kernel -> loader/data/kernel.zip
scripts/build.sh loader       # PPC loader -> loader/loader.dol
scripts/build.sh clean
scripts/build.sh all -j8      # extra arguments go to make
```

Always `clean` when switching toolchains. Objects are not namespaced per
toolchain, so stale ones get relinked and produce undefined references that
appear to be a libogc problem but are not.

---

## Installing what you built

1. Copy `loader/loader.dol` to `/apps/Nintendont/boot.dol` on your SD card or
   USB device, alongside `meta.xml` and `icon.png` from `nintendont/`.
2. Put GameCube images in `/games/`.
3. Launch from the Homebrew Channel.

There is no emulator path for testing: Dolphin does not emulate the ARM side,
so Nintendont has to be tested on real hardware (Wii, Wii mini, or Wii U in vWii
mode). Turning on **Settings → Advanced → Log to File** writes `/ndebug.log` to
the root of the storage device, which is the main diagnostic tool.

---

## Troubleshooting

| Symptom | Cause |
|---|---|
| `undefined reference to __syscalls` | libOGC 1.8.23 built against the wrong devkitPPC. Use r35-2. |
| `undefined reference to PPCDCacheFlushAsync` / `PPCDCacheInvalidate` | Objects compiled against a newer libogc. `scripts/build.sh clean` and rebuild. |
| `unknown type name 'raw_irq_handler_t'` | Building against libogc2. Use the pinned libOGC 1.8.23. |
| `cannot execute binary file` during `BIN2H` | The Linux helper is being run on Windows. Fixed in-tree; if you see it, your `kernel/asm/Makefile` predates that fix. |
| `Cannot create temporary file in C:\WINDOWS\` | `TMP`/`TEMP` are POSIX paths. `scripts/build.sh` sets native Windows ones; use it rather than calling make directly. |
| `elf2dol: No such file or directory` after a successful link | `$DEVKITPRO/tools/bin` is not on `PATH`. |
| `bin2o` / data files not embedded | devkitPPC r41+ in use. Use the pinned r35-2. |

`Build.bat` and `Build.sh` still exist and still work if your environment is
already set up the old way. They do not set any of the above up for you.
