#!/bin/bash

echo "codehandler.s"
$DEVKITPPC/bin/powerpc-eabi-gcc -nostartfiles -nodefaultlibs -Wl,-Ttext,0x93006000 -o ../kernel/bin2h/codehandler.bin codehandler.s
cd ../kernel/bin2h
$DEVKITPPC/bin/powerpc-eabi-strip --strip-debug --strip-all --discard-all -F elf32-powerpc codehandler.bin
$DEVKITPPC/bin/powerpc-eabi-objcopy -I elf32-powerpc -O binary codehandler.bin
bin2h codehandler.bin
rm -f codehandler.h
mv codehandler.h ..
