### Nintendont
A Wii Homebrew Project to play GC Games on Wii and vWii on Wii U

### Features:
-Works on Wii and vWii on Wii U  
-Full speed loading from an USB device, or a SD card  
-Loads 1:1 and compressed .ISO disc images  
-Loads games as extracted files (FST)  
-Play retail discs (Wii only)  
-Play backups from writable DVD media (Old Wii only)  
-Memory card emulation  
-Use real memory card (Wii only)  
-GBA-Link cable (Wii only)  
-Play audio via disc audio streaming  
-Bluetooth controller support (Classic Controller (Pro), Wii U Pro Controller)  
-HID controller support via USB  
-Custom button layout when using HID controllers  
-Cheat code support  
-WiiRd (Wii only)  
-Changeable configuration of various settings  
-Reset/Power off via button combo (R + Z + Start) (R + Z + B + D-Pad Down)  
-Advanced video mode patching, force progressive and force 16:9 widescreen  
-Auto boot from loader  
-Disc switching  
-Allow use of the Nintendo GameCube Microphone  
-Use the official Nintendo GameCube controller adapter  

### What Nintendont doesn't do yet:
-BBA/Modem support

### What Nintendont will never support:
-Game Boy Player

### Quick Installation:
Get the [loader.dol](loader/loader.dol?raw=true), rename it to boot.dol and put it in /apps/Nintendont/ along with the files [meta.xml](nintendont/meta.xml?raw=true) and [icon.png](nintendont/icon.png?raw=true).

### Build envionment
1. setup devkitpro and devkitPPC as described http://devkitpro.org/wiki/Getting_Started/devkitPPC
2. Setup devkitARM.
3. Set the following variables
export DEVKITPRO=/opt/devkitpro  # or your install location
export DEVKITPPC=$DEVKITPRO/devkitPPC
export DEVKITARM=$DEVKITPRO/devkitARM
4. checkout
5. (mac os only) Build bin2h
cd Nintendont/kernel/bin2h; sed -i.bak 's/malloc.h/stdlib.h/' main.c; ./make.sh; export PATH="$PATH":`pwd`; cd -
