### Nintendont
A Wii Homebrew Project to play GC Games on Wii and vWii on Wii U

> For this fork's Switch Pro integration, see [Switch Pro on Wii U](#switch-pro-on-wii-u).

### Features:
* Works on Wii and Wii U (in vWii mode)
* Full-speed loading from a USB device or an SD card.
* Loads 1:1 and shrunken .GCM/.ISO disc images.
* Loads games as extracted files (FST format)
* Loads CISO-format disc images. (uLoader CISO format)
* Memory card emulation
* Play audio via disc audio streaming
* Bluetooth controller support (Classic Controller (Pro), Wii U Pro Controller)
* HID controller support via USB
* Custom button layout when using HID controllers
* Cheat code support
* Changeable configuration of various settings
* Reset/Power off via button combo (R + Z + Start) (R + Z + B + D-Pad Down)
* Advanced video mode patching, force progressive and force 16:9 widescreen
* Auto boot from loader
* Disc switching
* Use the official Nintendo GameCube controller adapter
* BBA Emulation (see [BBA Emulation Readme](BBA_Readme.md))

### Features: (Wii only)
* Play retail discs
* Play backups from writable DVD media (Old Wii only)
* Use real memory cards
* GBA-Link cable
* WiiRd
* Allow use of the Nintendo GameCube Microphone

### What Nintendont will never support:
* Game Boy Player

### Switch Pro on Wii U

This fork adds wireless support for original **Nintendo Switch 1 Pro
Controllers** in GameCube games on a Wii U. It requires **Aroma**, an SD card,
and the matching Bloopair fork build with its sync plugin.

#### Download the matching prereleases

The hardware-tested source pair is:

| Component | Tested commit | Prerelease |
| --- | --- | --- |
| Nintendont | `889420e` | [`switch-pro-bloopair-v0.1.0-rc1`](https://github.com/GerwinVerkerk/Nintendont/releases/tag/switch-pro-bloopair-v0.1.0-rc1) |
| Bloopair, sync plugin and Koopair | `479479b` | [`switch-pro-nintendont-v0.1.0-rc1`](https://github.com/GerwinVerkerk/Bloopair/releases/tag/switch-pro-nintendont-v0.1.0-rc1) |

Download and install both matching prereleases. Do not use an upstream release,
an older repository binary or a different fork build for this integration.

#### Install, pair and play

1. Extract both installation ZIPs to the root of the same SD card. Nintendont
   must be installed as `sd:/apps/Nintendont/boot.dol`, with `meta.xml` and
   `icon.png` beside it. The matching Bloopair package installs its setup
   module, Aroma sync plugin and Koopair application.
2. Fully restart the Wii U so Aroma loads the new module and plugin.
3. In the Wii U menu, pair each original Switch 1 Pro Controller normally with
   the console and controller SYNC buttons. Existing working pairings can stay.
4. Start this fork's Nintendont and a GameCube game. In the game, press **A** on
   each Switch Pro to reconnect.

No Manual export, file copy or controller configuration is needed. The Bloopair
plugin maintains `sd:/wiiu/bloopair/nintendont-switch-pro.bin`; Nintendont reads
it at startup. Keep the SD card inserted. The file contains Bluetooth
authentication keys and must not be published or shared.

Only original Switch 1 Pro pairings are imported, up to four. Switch 2 Pro,
Joy-Con and third-party Switch controller protocols are not supported by this
Nintendont integration. Other controllers can remain paired in Bloopair without
using an export slot. This automatic route requires Aroma on Wii U; it does not
cover Tiramisu or an original Wii.

Player LEDs follow the assigned GameCube channel: player 1 lights LED 1,
player 2 lights LEDs 1+2, player 3 lights 1+2+3, and player 4 lights all four.
A physical GameCube controller can take an earlier channel and move the
Bluetooth controllers to later channels.

If reconnect fails, confirm Wii U-menu input, all matching fork files, the
enabled Aroma sync plugin and a writable SD card. Fully restart the console and
verify that the game launcher uses `sd:/apps/Nintendont/boot.dol`. Return to Wii
U mode and reconnect there before retrying; Nintendont reads the handoff only
when it starts.

The cleaned builds were hardware-tested with two Switch Pro Controllers in
Mario Kart: Double Dash!!, a searching PowerA in different activation orders,
correct player LEDs and input, and reassignment when a physical GameCube
controller takes adapter port 1. Four simultaneous Switch Pro Controllers and
the existing Wii U Pro Controller route were not tested in this validation.

This is a fork-specific integration. Compatibility with Bloopair's announced
upstream SD pairing storage has not yet been established. The current tested
route uses the companion Bloopair fork and sync plugin. See
[technical details](docs/switch-pro-incoming-minimal.md).

### Quick Installation:
> **Switch Pro through Bloopair:** do not use the repository `loader.dol` link
> below. Install both matching fork prerelease packages listed in
> [Switch Pro on Wii U](#switch-pro-on-wii-u). The upstream/repository binary
> does not provide the complete tested integration.

1. Get the [loader.dol](loader/loader.dol?raw=true), rename it to boot.dol and put it in /apps/Nintendont/ along with the files [meta.xml](nintendont/meta.xml?raw=true) and [icon.png](nintendont/icon.png?raw=true).
2. Copy your GameCube games to the /games/ directory. Subdirectories are optional for 1-disc games in ISO/GCM and CISO format.
   * For 2-disc games, you should create a subdirectory /games/MYGAME/ (where MYGAME can be anything), then name disc 1 as "game.iso" and disc 2 as "disc2.iso".
   * For extracted FST, the FST must be located in a subdirectory, e.g. /games/FSTgame/sys/boot.bin .
3. Connect your storage device to your Wii or Wii U and start The Homebrew Channel.
4. Select Nintendont.

### Compiling:
For compile Nintendont yourself, get the following versions of the toolchain compiling PPC tools:
* **devkitARM r53-1**
* **devkitPPC r35-2**
* **libOGC 1.8.23-1**

These versions can be downloaded here: https://www.mediafire.com/folder/j0juqb5vvd6z5/devkitPro_archives

On Windows, run the "Build.bat" batch script for build Nintendont.

On Unix, run the "Build.sh" script.

Please use these specific versions for compiling Nintendont, **because if you try to compile them on latest dkARM/dkPPC/libOGC, you'll get a lot of compiler warnings and your build will crash when attemping to return to Nintendont menu**, so be warned about that.

### Notes
* The Wii and Wii U SD card slot is known to be slow. If you're using an SD card and are having performance issues, consider either using a USB SD reader or a USB hard drive.
* USB flash drives are known to be problematic.
* Nintendont runs best with storage devices formatted with 32 KB clusters. (Use either FAT32 or exFAT.)
