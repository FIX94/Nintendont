#!/bin/bash

echo " CC          codehandler.s"
$DEVKITPPC/bin/powerpc-eabi-gcc -nostartfiles -nodefaultlibs -Wl,-Ttext,0x80001000 -o codehandler.bin codehandler.s
echo " CC          codehandleronly.s"
$DEVKITPPC/bin/powerpc-eabi-gcc -nostartfiles -nodefaultlibs -Wl,-Ttext,0x80001000 -o codehandleronly.bin codehandleronly.s
echo " STRIP       codehandler.bin"
$DEVKITPPC/bin/powerpc-eabi-strip --strip-debug --strip-all --discard-all -F elf32-powerpc codehandler.bin
echo " STRIP       codehandleronly.bin"
$DEVKITPPC/bin/powerpc-eabi-strip --strip-debug --strip-all --discard-all -F elf32-powerpc codehandleronly.bin
echo " OBJCOPY     codehandler.bin"
$DEVKITPPC/bin/powerpc-eabi-objcopy -I elf32-powerpc -O binary codehandler.bin
echo " OBJCOPY     codehandleronly.bin"
$DEVKITPPC/bin/powerpc-eabi-objcopy -I elf32-powerpc -O binary codehandleronly.bin
echo " BIN2H       codehandler.h"
../kernel/bin2h/bin2h codehandler.bin
echo " BIN2H       codehandleronly.h"
../kernel/bin2h/bin2h codehandleronly.bin
echo " RM          codehandler.bin codehandleronly.bin"
rm -f codehandler.bin codehandleronly.bin
