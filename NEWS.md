# Nintendont Changes

## v4.406

Changes since v3.304:

### General Game Support ###

* Added disc read limiting to emulate the proper speed of the disc drive.
* ARStartDMA fixes for several games.
* GXLoadTlut patch to fix "rainbow sky" bugs in Burnout and other games.
* Audio streaming improvements.
* Fixed some games that expect controllers to "always" be connected.
* Wii Remotes automatically disconnect after 20 seconds if no extension controllers are present.
* Timer patches to fix speed-up issues.
* Allow discs to be taken out and put back in on Wii when using the disc drive.
* Support for NTSC GameCube IPL v1.1 and v1.2.
* Fix for SRAM not being updated when using real memory cards on Wii.
* Option to modify video width and vertical video offset.
* Option to use PAL50 when using a forced video mode.
* Fixed auto-format of emulated memory card images for Japanese games.
* Fixed checks for multi-game discs. (collision with Call of Duty: Finest Hour)
* Fixed swapped audio streaming channels.
* Force-disable DI encryption on Triforce games to prevent random shutdowns.

### Game Fixes ###

* Fixed audio in Majora's Mask. (Ocarina confirm; credits sequence)
* Fixed the Japanese Pokémon Colosseum bonus disc.
* Fixed Force PAL60 on Luigi's Mansion.
* Fixed several demos that needed ARStartDMA exceptions.
* Added a new DSP patch for Pikmin 1 (US) demo.
* Fixed PAL50 issues on Ikaruga when not using the IPL.

### Cheat Codes ###

* Replaced kenobiwii with a cheat handler in the kernel, which saves memory and allows for more cheats to be used at once.

### Widescreen ###

New widescreen hacks for:

* Emulated N64 games (Ocarina of Time, Majora's Mask)
* Metroid Prime
* Metroid Prime 2: Echoes
* NFL Blitz 2002, 2003, Pro
* Skies of Arcadia Legends
* Sonic Heroes (US, JP, 10/8/2003 prototype)
* Sonic R (Sonic Gems Collection)
* Super Monkey Ball

### Force Progressive ###

New force progressive scan hacks for:

* Capcom vs. SNK 2 EO (US)
* Pac-Man World 3
* SpongeBob SquarePants CFTKK

### Triforce ###

Added support for:

* F-Zero AX (Revs. C and E)
* Virtua Striker 3 (JP, export)
* Virtua Striker 4 (JP, export)
* Virtua Striker 4 Ver 2006 (JP)
* Gekitou Pro Yakyuu (Revs. B and C)

### Storage Media ###

* The GPT partition scheme can now be used on HDDs and SD cards.
* Logical partitions are now detected.
* Rebased to FatFS R0.12_p3. This is now used for both loader and kernel.
* The exFAT file system is now supported.
* The Drive Access LED option now indicates when writing to SD/USB as well as reading.
* Timestamps are now set on emulated memory card images.

### Loader ###

* Rewrote the game list code to use less memory.
* Use the correct video mode on PAL systems. (fixes red tint when using RGB)
* Improved error messages if IOS58 could not be loaded.
* General UI improvements, including the ability to go back from the Game List to Device Selection.

### Other ###

* Disable PAL IPL when running on Wii U, since Wii U doesn't support its video mode.
