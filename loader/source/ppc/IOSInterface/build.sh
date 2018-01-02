#!/bin/bash

echo "_crt0.S"
$DEVKITPPC/bin/powerpc-eabi-gcc -DDEBUG -O1 -Wall -s -nostartfiles -nostdlib -mhard-float -c _crt0.S
echo "cache.c"
echo "debug.c"
echo "sock.c"
echo "usb.c"
$DEVKITPPC/bin/powerpc-eabi-gcc -DDEBUG -O1 -Wall -s -nostartfiles -nostdlib -mhard-float -T link.ld _crt0.o cache.c debug.c sock.c usb.c -o ../../../data/IOSInterface.bin
cd ../../../data
$DEVKITPPC/bin/powerpc-eabi-objcopy -S -O binary IOSInterface.bin
cd ../source/ppc/IOSInterface
