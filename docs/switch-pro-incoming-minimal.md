# Incoming original Switch Pro support

This branch supports original Nintendo Switch Pro Controllers paired on the
Wii U by Bloopair. Pairing still happens in Wii U mode; Nintendont only accepts
authenticated incoming reconnects after control transfers to vWii.

## Pairing handoff

Bloopair's Aroma package automatically maintains
`sd:/wiiu/bloopair/nintendont-switch-pro.bin`. The fixed 140-byte version-3
record contains the Wii U Bluetooth address and up to four entries containing
controller address, HCI-order link key, key type, controller type and VID/PID.
Bloopair updates it atomically when a pairing becomes available, replaces a
key after re-pairing and drops entries removed from the Wii U device database.
No address or link key is written to diagnostics.

Automatic maintenance requires the companion Bloopair change in
GaryOderNichts/Bloopair#212. Storage failures are retried by that component;
Nintendont itself only consumes a complete, validated record at startup. The
loader strictly validates record format, length, version and checksum, then
filters entries by the supported original-controller identity `057e:2009`.
An unsupported entry in an otherwise intact bundle is ignored; a malformed
supported entry or damaged envelope rejects the bundle.

Nintendont cannot call Bloopair after entering vWii: Bloopair patches IOSU's
IOS-PAD and exposes its extension through Wii U `/dev/usb/btrm`, while the
Nintendont loader runs under vWii IOS58 and uses FatFS. The loader therefore
validates the local handoff record, copies it to reserved shared memory, and
the ARM kernel consumes that copy. A legacy version-2 single-controller file
is accepted for rollback compatibility.

The sync component requires Aroma's Wii U Plugin System. Tiramisu has no WUPS
runtime and therefore does not provide the automatic handoff.

## Connection stateflow

1. The loader validates the record and copies it to reserved shared memory.
2. The ARM kernel registers one dedicated, idempotent incoming HID listener
   per imported controller address before the normal vWii listeners.
3. An A-wake connection is authenticated with that controller's imported link
   key and then encrypted.
4. Nintendont accepts incoming HID control PSM `0x11` and interrupt PSM `0x13`
   in either order. Callbacks mark the connection pending; a later Bluetooth
   tick verifies aggregate BTE state and both L2CAP PCBs before publication.
5. Each slot runs the twelve-command `hid-nintendo` initialization sequence.
   Commands require matching positive `0x21` responses and have bounded retry.
6. Device Info confirms identity. Native `0x30` input reports are translated
   to the existing `C_CCP`/`BTPadCont` contract and assigned to a free
   GameCube channel. Basic `0x3f` reports are not published.
7. Player LEDs follow the definitive GameCube channel and are reapplied after
   reconnect or reassignment without sending on every input report.

The incoming path does not perform inquiry, in-game pairing, outgoing HID or
remote-name detection. Pairing remains owned by Bloopair in Wii U mode.

Development-only hardware tracing is not written by release builds.

Unsupported controllers remain owned by Bloopair. Their presence, connection
state and activation order must not block imported original Switch Pro
Controllers or consume one of their available GameCube channels.
