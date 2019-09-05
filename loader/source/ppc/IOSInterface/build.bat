@echo off
:: Gets the directory that devkitPPC is installed to
setlocal
::set devpath=%DEVKITPPC:/=\%\
::set devpath=%devpath:~1,1%:%devpath:~2%
set devpath=C:\devkitpro\devkitPPC
echo _crt0.S
%devpath%\bin\powerpc-eabi-gcc -DDEBUG -O1 -Wall -s -nostartfiles -nostdlib -mhard-float -c _crt0.S
echo cache.c
echo sock.c
%devpath%\bin\powerpc-eabi-gcc -DDEBUG -O1 -Wall -s -nostartfiles -nostdlib -mhard-float -T link.ld _crt0.o cache.c sock.c -o ../../../data/IOSInterface.bin
cd ../../../data
%devpath%\bin\powerpc-eabi-objcopy -S -O binary IOSInterface.bin
cd ../source/ppc/IOSInterface
