# Compiled overlay regression tests

Build the project first with the upstream pinned toolchain: devkitARM r53-1,
devkitPPC r35-2 and libOGC 1.8.23-1. Export DEVKITPRO, DEVKITARM and DEVKITPPC.
Install Python 3.9+ and `python -m pip install -r tests/overlay/requirements.txt`.
Run `python tests/overlay/run.py` from any directory. NIN_SOURCE optionally
points to another built checkout; by default the tests use this repository.

The tests execute the compiled PPC overlay, VI hook, ARM kernel initialization
and save routines, and PPC exit sequence under Unicorn. The patch suite
extracts and compiles the actual VI validation case from Patch.c, with stubs
for installation side effects; it does not exercise the whole scanner.
The FAT backend is an in-memory stub, including failure injection. This is
not a Wii timing, DMA, real cache-coherency, FAT durability, or IOS test.

Six suites cover the existing 33 scenarios (including 144 mapping pairs),
new input regressions and rapid Y sequences, seven filesystem failures,
heap-safe deferred installation, generation wrap and repeated saves, raw
kernel entry/link rejection, exit handshake, and 16 VI candidate cases.
Logs and JSON summaries are ignored by Git. Generated C/ELF fixtures are
also ignored. No games or private machine paths are included.

The original local historical comparison with broken M15/M17 binaries is
recorded in the audit dossier; reproducing this branch does not require those
private historical build directories.

To regenerate the documentation preview, also install Pillow and run
`python tests/overlay/render_preview.py`. It captures the compiled renderer
with simulated inputs; it does not include a game image or claim a Wii test.
