#!/bin/bash

echo "codehandler.s"
$DEVKITPPC/bin/powerpc-eabi-gcc -nostartfiles -nodefaultlibs -Wl,-Ttext,0x80001000 -o ../kernel/bin2h/codehandler.bin codehandler.s
echo "codehandleronly.s"
$DEVKITPPC/bin/powerpc-eabi-gcc -nostartfiles -nodefaultlibs -Wl,-Ttext,0x80001000 -o ../kernel/bin2h/codehandleronly.bin codehandleronly.s
cd ../kernel/bin2h
$DEVKITPPC/bin/powerpc-eabi-strip --strip-debug --strip-all --discard-all -F elf32-powerpc codehandler.bin
$DEVKITPPC/bin/powerpc-eabi-objcopy -I elf32-powerpc -O binary codehandler.bin
$DEVKITPPC/bin/powerpc-eabi-strip --strip-debug --strip-all --discard-all -F elf32-powerpc codehandleronly.bin
$DEVKITPPC/bin/powerpc-eabi-objcopy -I elf32-powerpc -O binary codehandleronly.bin
bin2h codehandler.bin
bin2h codehandleronly.bin
rm -f codehandler.bin codehandleronly.bin
mv -f codehandler.h ..
mv -f codehandleronly.h ..
