# Nintendont Changes

## v5.460

Changes since v5.460:

### Major Changes ###

* Skip USB drives that don't have a valid MBR or UStealth MBR signature.
  This usually happens on Wii U setups where the user has both a Wii U drive
  and a FAT32 drive for Wii and GameCube. This allows the user to keep both
  drives connected when running Nintendont.
* Support for injected Wii VC on Wii U. This allows use of the Wii U GamePad
  as Player 1 in addition to storing GameCube disc images on Wii U storage.
* New option to skip the GameCube IPL.
* Support for Wii Remote rumble when using Classic Controller.
* New "Game Info" screen that shows information about the selected disc image.

### Other Changes ###

* More patches for various games, including timing and video mode fixes.
* Fix BMX XXX 480p mode, again. (Broken since v4.428, commit e6e1c6a)
* Datel AGP fixes.
* The Game List now shows the filename of the selected game. This is useful
  for distinguishing between e.g. different revisions of the same game.

## v4.430

Changes since v4.406:

### Major Changes ###

* Added support for Hermes uLoader compressed ISO (CISO) format disc images.
  CISO saves space like shrunken GCMs, but it maintains the original file
  offsets internally, which preserves the original disc read timing.
* Ported libfat's disk cache to FatFS in the loader. This fixes a hang seen
  on some systems when creating memory card images.
* Use the bi2.bin region code instead of the game ID. This fixes region
  detection for prototype games that use the RELSAB ID as well as mutli-language
  PAL releases that use 'X', 'Y', and 'Z'.
* Added a built-in MD5 verifier for 1:1 disc images. The MD5 verifier uses a
  database based on GameTDB. The database must be downloaded manually on the
  "Update" screen.

### Other Changes ###

* The internal updater was accidentally broken with the switch to FatFS
  in v4.406.
* Improved support for certain types of third-party Wii U Pro controllers.
* Fixes for some patches, including the code handler.
* Nintendont will no longer display any messages if it is loaded by an external
  loader, unless an error occurs.
* The loader's background image is now adjusted for 16:9 display if the Wii's
  system widescreen setting is enabled.
* Disc images no longer have to be in a subdirectory within /games/. For example,
  you can have a file /games/Melee.iso and it will be detected. (A subdirectory
  is still required for 2-disc games.)
* File extensions .iso, .gcm, .cso, and .ciso are supported.
* The game list now colorizes game names based on their file format:
  * Black: 1:1 full rip
  * Dark Brown: Shrunken
  * Green: Extracted FST
  * Blue: CISO
  * Purple: Multi-Game
  * Dark Yellow: Oversized
* Fixed an issue where pressing the Home button in the Settings menu after
  changing a setting crashed the loader.
* Grayed out settings that don't apply to the current system, e.g. Wii U
  Widescreen on a regular Wii.
* Added descriptions for some settings. When selected, the description will
  appear in the lower-right quadrant of the screen.
* Added more widescreen exceptions for games that have built-in widescreen settings.
* Improved support for multi-game discs. Previously, Nintendont didn't handle
  accessing data past 4 GB. Nintendont now uses 64-bit addressing for multi-game,
  so multi-game images on exFAT partitions now work perfectly. Multi-game on
  DVD-Rs is still somewhat problematic.
* Reduced memory usage and improved performance when formatting a new memory card.
* Improved usability in the Update menu.

### Triforce Improvements ###

* Rumble is now supported in Mario Kart Arcade GP 1 and 2.
* New "Triforce Arcade Mode" option that disables free play.
  Move the C stick in any direction to insert a coin.

### Low-Level Changes ###

* All subprojects now use Makefiles. The original Build.bat and Build.sh files
  are now wrappers around `make`.
* Split the game and settings menus into different functions. The current menu
  state is saved within a context struct.

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
