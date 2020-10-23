### Nintendont with XBOX360 controller support
* Only XBOX360 **wired version** is supported (VID=0x045e, PID=0x028e)
* You control player 1.
* Tested on Wii with Super Mario Sunshine on SD card.

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
    5. Set `EndpointOut=0` in`/controller/045e_028e.ini`. This will disable LED and rumble.<br>
    Also use this settings if your Wii crashes. (Rare crashes are normal, though.)
  
### Turn on rumble ###
* If it doen't work by default, set `EndpointOut=2` in `SD:/controller/045e_028e.ini`.<br>
  The default is `EndpointOut=1` which is surprisingly correct for my controller ;-)

### Debugging ###
* Enable _Debugger_ and _Log_ in Nintendont-XBOX360 settings.
* Check log file `/ndebug.log`.
* The log file is not always written: Best practice is to start a game, then plug in your controller, then quit.

#### Download ###
* [Nintendont-XBOX360-1.0.zip](https://github.com/revvv/Nintendont-XBOX360/releases/download/1.0/Nintendont-XBOX360-1.0.zip)
* Unpack it to your SD card.
* Put games in folder `/games`
* *Optional:* Configure button layout in `/controller/045e_028e.ini`.

### Compile
Get these versions: devkitppc r29-1, devkitarm r47 and libogc 1.8.16 and execute _make_. 

### Disclaimer
This software comes without any warranty. I am not responsible for any damage to your devices. Please make backups!

