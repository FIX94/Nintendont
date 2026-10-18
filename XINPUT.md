# Wired Xbox 360 USB controller support

This change recognizes `045e:028e` wired Xbox 360-style controllers through
IOS58's `/dev/usb/ven` interface. It reuses the handle opened for kernel USB
storage and publishes normalized reports through the existing HID pad path.
An ordinary USB HID controller found at startup takes precedence.

The included `controllerconfigs/045E_028E.ini` supplies the default GameCube
layout: matching A/B/X/Y labels, right bumper for Z, Start for Start, and
analog triggers with a digital threshold. Guide, Back and stick clicks are
unmapped. Rumble is not implemented. The kernel normalizes axes itself;
the profile's legacy endpoint/inversion hints are not used by the parser.

Interrupt transfers are asynchronous and submissions are spaced about 8 ms
apart. Descriptor and attach/control operations remain synchronous. The code
checks descriptor bounds and complete input-report lengths; an immediately
rejected submission follows the same bounded retry path as a failed callback.
The device-specific large-stick-jump filter holds at most eight reports before
accepting a new position. It can delay legitimate fast movement and needs
testing beyond the original pad.

## Scope and validation

The supporting Wii test history includes Wind Waker with a controller reporting
`045e:028e`, using a development build that also contained the menu overlay.
The user reported that setup working. This is not a claim that the final
standalone PR head or every title has been tested on hardware.

The final standalone source is built with devkitARM r53-1, devkitPPC r35-2 and
libOGC 1.8.23-1. `tests/xinput/` executes the relevant ARM routines for malformed
descriptors, short reports, normalization, bounded filtering and polling errors.
The host fixture cannot verify IOS scheduling or real USB storage concurrency.

Only the ID above is currently selected; other XInput IDs are not automatically
supported. Wireless receivers and Xbox One/Series protocols are outside scope.
USB storage must have initialized the shared ven handle; SD-only use is not
established. This adds one controller slot, not multiple USB pads. Mixed HID
hotplug, physical reconnects, exit cleanup, vWii, sustained game loading and the
final report-length/cache changes still need dedicated hardware checks.

The IOS interrupt completion byte-count convention is also represented by
[Dolphin's USB transfer implementation](https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/Core/IOS/USB/LibusbDevice.cpp).
That source cross-check is not a substitute for checking the final build on Wii.
