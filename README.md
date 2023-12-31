## Nintendont - libertyernie's build (preprocessor flags version)

Available preprocessor flags in this branch (define these in NintendontVersion.h):

* `LI_XBOX360`: includes all extra code from https://github.com/revvv/Nintendont-XBOX360 to support Xbox 360 controllers and others (see below - note that some supporting code is included either way)
* `LI_NONUNCHUK`: removes Nunchuk support
* `LI_GAMEPADASCCPRO`: removes Wii U GamePad mapping code and instead reuses the Classic Controller Pro code path
* `LI_NOSWAP`: removes the controller shortcuts that allow you to swap buttons (Y/B vs. B/A)
* `LI_NOEXIT`: removes the ability to exit Nintendont without turning off or resetting the console
* `LI_NORESET`: removes the ability to reset the game with a controller button combo (at least on certain controllers)
* `LI_CUSTOM_CONTROLS`: adds special controller overrides for the Classic Controller and Classic Controller Pro (see below)
* `LI_BASE64`: lets you load a base64-encoded nincfg.bin from meta.xml (also see [NinCFGEditor](https://github.com/libertyernie/NinCFGEditor))
* `LI_SHOULDER`: tweaks the button mappings on certain controllers
    * Classic Controller:
        * L -> L (analog)
        * R -> R (analog)
        * ZL -> half L press (0x7F)
        * ZR -> half R press (0x7F)
        * Home -> Start (if `LI_NOEXIT` is used)
        * Select -> Z
    * Classic Controller Pro:
        * ZL -> full L press (0xFF)
        * ZR -> full R press (0xFF)
        * L -> half L press (0x7F)
        * R -> half R press (0x7F)
        * Home -> Start (if `LI_NOEXIT` is used)
        * Select -> Z
* `LI_SHOULDER_DIRECT`: tweaks the button mappings on certain controllers
    * Classic Controller / Classic Controller Pro:
        * L -> L (analog passthrough)
        * R -> R (analog passthrough)
        * ZL -> Y
        * ZR -> Z
* `LI_ANALOG_SHOULDER_FULL`: simulates a full analog L/R press on the Classic Controller whenever a digital (full) press is detected

To build on Windows, you might need to set the "windows" variable so the build process can find zip.exe:

    $ windows=1 make

You'll also want to make sure the devkitpro folder with libwinpthread-1.dll is in your PATH.

.dol files (if any) are in the Releases section on GitHub.

### Custom controls

When the preprocessor flag `LI_CUSTOM_CONTROLS` is enabled, the following changes will be made to
Classic Controller and Classic Controller Pro button mappings:

* The Legend of Zelda: Four Swords Adventures
    * D-pad unmapped
    * D-pad -> left analog stick, full tilt
    * If `LI_SHOULDER_DIRECT` is enabled:
        * Select -> X
* Mario Party 4
    * D-pad unmapped
    * D-pad -> left analog stick, full tilt

If `LI_CUSTOM_CONTROLS` and `LI_SHOULDER` are both enabled, the following changes will be made as well:

* Bust-a-Move 3000
    * Either L button, at least 25% -> full L press
    * Either R button, at least 25% -> full R press
    * Diagonal inputs prevented on the D-pad (vertical preferred)
    * Diagonal inputs prevented on the left stick (reduced to up/down/left/right)
* Midway Arcade Treasures 3
    * Large L button, at least 25% -> full L press
    * Large R button, at least 25% -> full R press
* Nintendo Puzzle Collection
    * Analog trigger inputs supressed unless one of the corresponding shoulder buttons is digitally pressed
* Spy Hunter
    * Default shoulder buttons unmapped
    * Default Z button unmapped
    * Default X and Y buttons unmapped
    * Either L button (at least 25%) -> L
    * Either R button (at least 25%) -> R
    * Small L or R button -> Z (in addition to L or R)
    * X -> Y
    * Y or Select -> X
* Super Smash Bros. Melee
    * D-pad unmapped
    * Default L, R, and Z buttons unmapped
    * Analog L and R inputs reset to native controller's inputs
    * D-pad -> left analog stick, 50% tilt
    * Large L button -> full L press
    * Small L button -> 25% L press
    * Large R button -> full R press
    * Small R button -> Z
    * Select -> D-pad up
* Super Mario Sunshine (Classic Controller Pro only)
    * Default R button unmapped
    * L -> 100% analog L press (no digital press)
    * R -> 100% analog and digital R press
    * ZR -> 100% analog R press (no digital press)
* Luigi's Mansion (Classic Controller Pro only)
    * Default shoulder buttons unmapped
    * ZL or ZR -> R
    * L or R -> 100% L press (no digital press)
    * L and R -> digital L press
* Mario Kart: Double Dash!!
    * Default shoulder buttons unmapped
    * D-pad unmapped
    * Default Y button unmapped
    * D-pad -> left analog stick, full tilt
    * Y + left analog stick (more than 25% left or right) -> left analog stick 100% left or right + A + L + R
    * Up, Down, large L button, or at least 25% L press -> X
    * Left -> digital L press
    * Right -> digital R press
    * Small L or R buttons -> Z

## Nintendont
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


### Nintendont with XBOX360 controller support
* Only XBOX360 **wired version** is supported (VID=0x045e, PID=0x028e)
* You control player 1.
* Tested on Wii with Super Mario Sunshine on SD card.
* If the button layout is not perfect, try _L+Back_ to turn buttons quarter clockwise. (Nintendont feature.)

### HOWTO ###
* Connect your XBOX360 controller then start Nintendont-XBOX360. (Order is optional.)
* The XBOX360 LED 1 should now light up and the gamepad should react.<br>
  If you start a game the WiiMote LED says player 2.
* If your controller doesn't work try one of these options:
    1. Restart Nintendont-XBOX360 or your Wii.
    2. Replug your controller.
    3. Plug in your controller after you have started a game. 
    4. If it is still not working, try my USB gamepad tester [RetrodeTest](https://github.com/revvv/snes9xgx-retrode/releases/download/0.5/RetrodeTest-0.2.zip)
    to check if your Gamepad works at all.
    5. Set `EndpointOut=0` in`/controller/045e_028e.ini`. This will disable rumble and LED will flash.<br>
    Also use this settings if your Wii crashes. (Rare crashes are normal, though.)
  
### Turn on LED and rumble ###
* If the LED keeps flashing, this means that the LED cannot be set and also rumble will not work.
  Try to set `EndpointOut=2` in [/controller/045e_028e.ini](https://github.com/revvv/Nintendont-XBOX360/blob/master/controllerconfigs/045e_028e.ini).<br>
  The default is `EndpointOut=1` which is surprisingly correct for my controller ;-)<br>
  There are controllers which are not compatible with these settings. You could try the values 1-8.<br>
  Maybe check `lsusb -v -d 045e:028e` and grep for `OUT`. [Example output](https://gist.github.com/rombert/6902ab691e12ab478e31)

### Debugging ###
* Enable _Debugger_ and _Log_ in Nintendont-XBOX360 settings.
* Check log file `/ndebug.log`.
* The log file is not always written: Best practice is to start a game, then plug in your controller, then quit.

#### Download ###
* [Release](https://github.com/revvv/Nintendont-XBOX360/releases/)
* Unpack it to your SD card.
* Put games in folder `/games`
* *Optional:* Configure button layout in [/controller/045e_028e.ini](https://github.com/revvv/Nintendont-XBOX360/blob/master/controllerconfigs/045e_028e.ini).<br>
  __NEW:__ Fixed XBOX360 rumble for vWii<br>
  __NEW:__ Support 3rd party WiiMote + Nunchuk (labeled with "NEW2in1" and "Motion plus", distributed by Haiwai Consulting/Tiger-Zhou UG)
  
### Compile
Get these versions: devkitppc r29-1, devkitarm r47 and libogc 1.8.16 and execute _make_. 

### Disclaimer
This software comes without any warranty. I am not responsible for any damage to your devices. Please make backups!

