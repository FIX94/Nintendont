# Native Switch Pro Bluetooth prototype

This branch adds a minimal native Bluetooth path for an original Nintendo
Switch 1 Pro Controller. It does not run Bloopair inside vWii and does not use a
USB adapter.

## What the code does

- Keeps the existing Wii Remote, Classic Controller and Wii U Pro paths intact.
- Reads link-key addresses already stored in the Wii U Bluetooth controller.
- Retains Nintendont's normal vWii `CONF_GetPadDevices` registrations.
- Listens for stored-key addresses that are absent from vWii SYSCONF and probes
  those devices as Switch Pro controllers.
- Sends Switch subcommands for device information, full `0x30` input reports
  and the player LED.
- Parses both full `0x30`/`0x21` reports and fallback `0x3f` basic reports.
- Maps A/B/X/Y, D-pad, both sticks, L/R/ZL/ZR, Plus and Home through
  Nintendont's existing GameCube controller path.
- Reopens the Bluetooth listeners after a Switch Pro disconnect.

Rumble is intentionally not part of this first playable build. Capture and the
two stick-click buttons have no GameCube equivalents and are ignored. Minus is
reserved for a future mapping option.

## Pairing and handoff finding

Nintendont normally obtains device addresses from vWii SYSCONF in
`loader/source/main.c`, then registers only those addresses in `kernel/BT.c`.
Its lwBT stack separately obtains stored link keys from the Bluetooth controller
through the HCI `Read Stored Link Key` command.

Bloopair patches Wii U IOS-PAD. During pairing it calls
`registerNewDevice(...)` and `BTM_WriteStoredLinkKey(...)`. It does not copy a
controller entry into vWii SYSCONF. This branch bridges that missing device-list
step by listening on extra stored-key addresses.

Whether a Bloopair-created key remains visible to Nintendont after the Wii U to
vWii transition can only be established on real hardware. A successful build
does not prove this handoff. If no extra key appears, a separate pairing/import
helper will be needed; the parser, protocol initialization and mapping remain
usable.

Protocol reference: Bloopair commit
`a8b8aad07cf4df51c34e31e1694b3e2a64517de4`, especially
`ios/ios_pad/source/controllers/switch_controller.c`. Both projects use GPLv2
compatible licensing.

## Install on Wii U

1. Keep a copy of the currently working `sd:/apps/Nintendont/boot.dol`.
2. Install and boot Aroma with Bloopair as usual.
3. In the Wii U Menu, press the console SYNC button and the small SYNC button on
   the original Switch Pro Controller. Confirm that Bloopair can use it there.
4. Turn the Switch Pro Controller completely off before entering vWii, so the
   Wii U-side stack does not keep the active connection.
5. Copy this branch's `loader/loader.dol` to
   `sd:/apps/Nintendont/boot.dol`.
6. In USB Loader GX, keep the GameCube loader set to Nintendont and ensure its
   Nintendont path points to `sd:/apps/Nintendont/boot.dol`.
7. Start Nintendont directly once for the first test. After the game list or a
   game is visible, press a face button on the Switch Pro Controller to make it
   reconnect.

Rollback: restore the backed-up `boot.dol`. Pairing data is not modified by this
build.

## Hardware test protocol

Record pass/fail and any LED behavior for every step:

1. **Pairing persistence:** pair under Aroma/Bloopair, power the controller off,
   enter vWii/Nintendont, then power it on. Expected: one player LED becomes
   steady within 20 seconds.
2. **Face buttons:** verify A, B, X and Y individually in a known game or input
   test screen.
3. **D-pad:** verify all four directions, including diagonals.
4. **Left stick:** verify center, full range, both axes and absence of drift.
5. **Right stick/C-stick:** verify center, full range and both axes.
6. **Shoulders:** verify ZL/ZR as the GameCube analog L/R triggers; L acts as
   Nintendont's half-press modifier and R maps to GameCube Z, matching the
   existing Wii U Pro mapping.
7. **System buttons:** verify Plus maps to Start and Home exits through
   Nintendont's existing return path.
8. **Reconnect:** power the controller off during play, wait five seconds,
   power it on and verify input resumes without restarting Nintendont.
9. **Regression:** connect a Wii Remote + Classic Controller or Wii U Pro
   Controller and verify its existing input path still works.
10. **USB Loader GX:** repeat game launch through USB Loader GX after the direct
    Nintendont test succeeds.
11. **Return:** exit and confirm a clean return to the configured vWii/Wii U
    menu rather than a hang or black screen.

## Verification status

Verified without console hardware:

- Host parser/mapping/subcommand tests pass: `make -C tests clean all`.
- ARM/PPC source compiles and links to `loader/loader.dol`.
- Kernel link succeeds with the added code.

Still requires Wii U hardware:

- Visibility of the Bloopair-created stored link key in vWii.
- Bluetooth connection and Switch protocol initialization.
- Real stick calibration/range behavior.
- Reconnection, Wii-controller regression and return-to-menu behavior.

Build used for the initial artifact:

- Base Nintendont commit: `bbc0a208e81d6ab0a828a431dc0b2ba6fc06aec5`
- devkitARM release 55 / GCC 10.2.0
- devkitPPC release 38 / GCC 10.2.0

Upstream recommends devkitARM r53-1 and devkitPPC r35-2. The linked archives
contain Windows executables, so this Linux build used the closest reproducible
devkitPro container available. That compiler difference is an additional item
for the hardware test, especially return-to-menu behavior.
