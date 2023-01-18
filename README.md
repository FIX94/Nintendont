## Nintendont - libertyernie's build (preprocessor flags version)

Available preprocessor flags in this branch (define these in NintendontVersion.h):

* `LI_NOSWAP`: removes the controller shortcuts that allow you to swap buttons (Y/B vs. B/A)
* `LI_NOEXIT`: removes the ability to exit Nintendont without turning off or resetting the console
* `LI_CUSTOM_CONTROLS`: adds special controller overrides for the Classic Controller, Classic Controller Pro, and Wii U GamePad
    * The Legend of Zelda: Four Swords Adventures
	    * D-Pad => Left Stick
		* Select => D-Pad Down
    * Super Puzzle Bobble / Bust-A-Move 3000 / Bust-A-Move All-Stars
        * Both left shoulder buttons -> full L press
        * Both right shoulder buttons -> full R press
        * D-pad diagonals -> D-pad horizontals
        * Analog stick diagonals -> analog stick horizontals or verticals (whichever is closer)
* `LI_BASE64`: lets you load a base64-encoded nincfg.bin from meta.xml (also see [NinCFGEditor](https://github.com/libertyernie/NinCFGEditor))
* `LI_SHOULDER`: tweaks the button mappings for the Classic Controller, Classic Controller Pro, and Wii U GamePad
    * Large left shoulder button: full L press
    * Small left shoulder button: half L press
    * Large right shoulder button: full R press
    * Small right shoulder button: half R press
    * Home: Start (only if `LI_NOEXIT` is also used)
    * Select / Minus: Z

To build on Windows, you might need to set the "windows" variable so the build process can find zip.exe:

    $ windows=1 make

You'll also want to make sure the devkitpro folder with libwinpthread-1.dll is in your PATH.

.dol files (if any) are in the Releases section on GitHub.

### Nintendont
A Wii Homebrew Project to play GC Games on Wii and vWii on Wii U

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

### Quick Installation:
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
