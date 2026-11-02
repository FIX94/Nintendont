# MemCard PRO / FlipperMCE hardware test plan

The host tests in this directory cover packet contents, slot selection, retry
limits and EXI cleanup with a mocked bus. Everything below needs real hardware.

Setup: an original Wii with GameCube ports, a FlipperMCE/GCMCE in Slot A with
Game ID enabled, **Memcard Emulation** off, and Nintendont's **Memcard Game ID**
option on unless a scenario says otherwise. **Back up the card's SD first.**
A USB Gecko in Slot B shows the `MemCardPro:` log lines, but it disables the
Slot B tests.

| Scenario | Expected behavior |
|---|---|
| Fresh install or older nincfg.bin | Memcard Game ID shows Off, and the device doesn't switch |
| Memcard Game ID turned on, Nintendont restarted | Still On, and the device switches |
| Settings menu | Cursor reaches the option in both directions, and every description fits on screen |
| Direct launch of game A | The device selects game A's card before the game reads it |
| Direct launch of game B | The device switches to game B's card |
| Relaunch game A | Game A's card and saves are back |
| First launch of a game with no card yet | Card is created, and the game sees it |
| Same game through modified USB Loader GX | Same card as a direct launch |
| Existing `Game2Folder.ini` mapping | The mapped folder is selected |
| Ordinary Nintendo memory card | Saves normally |
| No card inserted | Game launches without a delay |
| Memcard Emulation on | Virtual saves unchanged, device doesn't switch |
| Hybrid EXI on, device in Slot B | Slot B switches |
| Hybrid EXI on, microphone in Slot B | Microphone still works |
| Compatible device in Slot B | Switches |
| USB Gecko in Slot B | Logging still works, device in Slot A still switches |
| Game with a nonzero revision | Revision-specific ID on MemCard PRO |
| Multi-game image | ID of the game picked from the list, not the image |
| Extracted FST and CISO images | Same card as the same game as an ISO |
| Physical disc | Same card as the same game as an ISO |
| Autoboot from nincfg.bin | Same as a direct launch |
| Wii U/vWii | Game launches, and no EXI traffic is attempted |

Timing assumptions to check:

- A device that switches cards is expected to stop answering the ID query
  within 500ms and to answer again when it's done. This comes from the
  FlipperMCE source. MemCard PRO behavior is unconfirmed.
- If a device switches without ever leaving the bus, the loader stops waiting
  after 500ms. The kernel takes at least 2 more seconds to start the game.
- The wait gives up after 15 seconds, then the game launches anyway.
