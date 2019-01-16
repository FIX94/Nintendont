## Project Slippi
This is a fork of Nintendont is used to support [Project Slippi](https://github.com/JLaferri/project-slippi)
on Wii hardware, and is specifically built for booting into _Super Smash Bros. Melee_
and applying specialized patches. Because this is the case, please be aware that there
are *no guarantees* that this fork will act predictably in any other circumstances.

**TODO:**

- [ ] Option for controlling networking and/or writes to USB
- [ ] Option[s] for common Gecko codes (UCF, Melee's OSReport handler/debugger, etc.)
- [ ] Support console discovery by letting the Slippi server announce presence
      on LAN (via UDP perhaps) if a client is not connected and consuming data

## Requirements/Instructions
In order to properly boot into Melee, you'll need a Vanilla NTSC v1.02 ISO and
a GCT file prepared with the "Slippi Recording" codeset.

Currently we require that you boot the game from SD card and leave the USB
drive free for Slippi to write data to. Assuming that you're booting Nintendont
from the Homebrew Channel, your SD card should include the following files and
folders:

```
/ SD Card Root
├── apps/
│   └── Nintendont/
│       ├── boot.dol		# Nintendont Slippi DOL
│       └── meta.xml		# Homebrew Channel title data
└── games/
    └── GALE01/
        ├── GALE01.gct		# GCT including the "Slippi Recording" codeset
        └── game.iso		# Vanilla NTSC v1.02 ISO
```

In order to write Slippi replay files to USB, you'll have to create a `Slippi/`
folder in the root of your USB drive like this:

```
/ USB Drive Root
└── Slippi/			# Folder for Slippi replay data
```


## Implementation Overview
This is a running list of noteworthy changes from upstream Nintendont:

- Emulate a Slippi EXI device on slot A (channel 0, device 0) for consuming DMA
  writes from the "Slippi Recording" Gecko codeset
- Force disable of TRI arcade feature (using freed space to store Melee gamedata)
- Implement [an approximation of the] standard POSIX socket API for networking
- Inject a Slippi file thread for writing gamedata to a USB storage device
- Inject a Slippi server thread for communicating with remote clients over TCP/IP


## Development Wisdom
Some useful notes for developers who might want to hack on this.

### Wii Networking
Network initialization should succeed assuming that the user has a valid network
connection profile, and that IOS is able to actually establish connectivity.

Note that Melee will crash/freeze on boot if network initialization is not delayed
by some amount of time (currently it seems like only a few seconds of delay is
sufficient). This is probably due to an increased amount of disc reads during
boot, for which Melee expects data to be returned in a timely manner from
Nintendont's DI thread.

### Dealing with IOS `ioctl()`
When issuing ioctls to IOS, pointer arguments should be be 32-byte aligned.
You can either (a) get an pointer on the heap with the `heap_alloc_aligned()`
syscall, (b) put something on the `.data` section with the `aligned(32)` attribute,
or (c) use the `STACK_ALIGN` macro to align structures on the stack.

### ARM/PPC Cache Weirdness
Exercise caution when trying to coordinate reads/writes between PowerPC and the
ARM coprocessor. They keep separate caches, and there may be some situations
where you need to use cache control instructions to make sure that they don't
have conflicting views of main memory.

### Logging to SDHC
Typically, after booting into a game, `dbgprintf()` calls are somewhat unreliable
if you're writing logs to an SD card (although, logging to USB Gecko apparently
works pretty well). Additionally, we also believe there are cases where attempts to
write on SDHC may make things unstable and cause crashes in PPC-land. Keep this in
mind if you're planning on making `dbgprintf()` calls during runtime in Melee.

### In-game Logging
The `ppc_msg()` macro defined in `kernel/SlippiDebug.h` can be used to write messages
on the screen. This may be useful if you don't have a USB Gecko, or if you don't
want to bother with logging to SD card. At the moment, in order to see any output,
you'll need to have a GCT prepared with the on-screen display Gecko codeset 
(graciously provided by UnclePunch).

