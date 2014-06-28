#!/bin/bash

echo "PadReadGC.c"
$DEVKITPPC/bin/powerpc-eabi-gcc -O1 -s -nostartfiles -T openstub.ld PADReadGC.c -o ../../data/PADReadGC.bin
cd HID

echo "PadReadHID.c"
$DEVKITPPC/bin/powerpc-eabi-gcc -O1 -s -nostartfiles -mhard-float -T openstub.ld PADReadHID.c -o ../../../data/PADReadHID.bin
cd ../../../data
$DEVKITPPC/bin/powerpc-eabi-objcopy -S -O binary PADReadGC.bin
$DEVKITPPC/bin/powerpc-eabi-objcopy -S -O binary PADReadHID.bin
cd ../source/ppc
