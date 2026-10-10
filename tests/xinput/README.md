# XInput regression checks

Use the project's pinned devkitARM r53-1 and Python 3.8 or later:

```sh
python -m pip install -r tests/xinput/requirements.txt
export DEVKITARM=/path/to/devkitARM
python tests/xinput/test_xinput.py
```

On PowerShell set `$env:DEVKITARM` to the toolchain directory instead.

The fixture extracts the actual parser, normalizer, read and polling functions
from `kernel/HID.c`, compiles them as big-endian ARM and executes them in Unicorn.
It checks 337 descriptor cases with read bounds, truncated reports with stale
packet contents, normalized sticks and buttons, recovery after a run of spike
rejections, immediate submission failures and the one-request polling cadence.

IOS transfers, cache operations, device enumeration and timers are stubbed.
This does not validate real USB storage concurrency, physical disconnects,
hotplug arbitration with another HID controller, vWii or cache visibility.
The complete Nintendont build is a separate required check.
