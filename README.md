## Nintendont - libertyernie's build (preprocessor flags version)

Available preprocessor flags in this branch (define these in NintendontVersion.h):

* `LI_NOSWAP`: removes the controller shortcuts that allow you to swap buttons (Y/B vs. B/A)
* `LI_NOEXIT`: removes the ability to exit Nintendont without turning off or resetting the console
* `LI_NORESET`: removes the ability to reset the game with a controller button combo (at least on certain controllers)
* `LI_CUSTOM_CONTROLS`: adds special controller overrides for the Classic Controller, Classic Controller Pro, and Wii U GamePad
    * The Legend of Zelda: Four Swords Adventures
	    * D-Pad => Left Stick
    * Mario Party 4
	    * D-Pad => Left Stick
    * Super Puzzle Bobble / Bust-A-Move 3000 / Bust-A-Move All-Stars
        * Both left shoulder buttons (on Classic Controller, pressed down at least 25%) -> full L press
        * Both right shoulder buttons (on Classic Controller, pressed down at least 25%) -> full R press
        * D-pad diagonals -> D-pad horizontals
        * Analog stick diagonals -> analog stick horizontals or verticals (whichever is closer)
    * Midway Arcade Treasures 3
        * Classic Controller L (pressed down at least 25%) -> full L press
        * Classic Controller R (pressed down at least 25%) -> full R press
    * Nintendo Puzzle Collection
        * on Classic Controller, L and R do not activate unless one of the corresponding shoulder buttons is pressed all the way (digitally)
* `LI_BASE64`: lets you load a base64-encoded nincfg.bin from meta.xml (also see [NinCFGEditor](https://github.com/libertyernie/NinCFGEditor))
* `LI_SHOULDER`: tweaks the button mappings on certain controllers
    * Classic Controller:
        * L -> L (analog)
        * R -> R (analog)
        * ZL -> half L press (0x7F)
        * ZR -> half R press (0x7F)
        * Home -> Start (if `LI_NOEXIT` is used)
        * Select -> Z
    * Classic Controller Pro / Wii U GamePad:
        * ZL -> full L press (0xFF)
        * ZR -> full R press (0xFF)
        * L -> half L press (0x7F)
        * R -> half R press (0x7F)
        * Home -> Start (if `LI_NOEXIT` is used)
        * Select -> Z
* LI_ANALOG_SHOULDER_FULL: simulates a full analog L/R press on the Classic Controller whenever a digital (full) press is detected

To build on Windows, you might need to set the "windows" variable so the build process can find zip.exe:

    $ windows=1 make

You'll also want to make sure the devkitpro folder with libwinpthread-1.dll is in your PATH.

.dol files (if any) are in the Releases section on GitHub.

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

