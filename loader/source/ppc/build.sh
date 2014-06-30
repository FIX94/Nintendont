#!/bin/bash

echo "PadReadGC.c"
$DEVKITPPC/bin/powerpc-eabi-gcc -O1 -s -nostartfiles -mhard-float -T openstub.ld PADReadGC.c -o ../../data/PADReadGC.bin
cd ../../data
$DEVKITPPC/bin/powerpc-eabi-objcopy -S -O binary PADReadGC.bin
cd ../source/ppc
