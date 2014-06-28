@echo off
:: Gets the directory that devkitPPC is installed to
setlocal
set devpath=%DEVKITPPC:/=\%\
set devpath=%devpath:~1,1%:%devpath:~2%

echo PadReadGC.c
%devpath%\bin\powerpc-eabi-gcc -O1 -s -nostartfiles -T openstub.ld PADReadGC.c -o ../../data/PADReadGC.bin

echo PadReadHID.c
cd HID
%devpath%\bin\powerpc-eabi-gcc -O1 -s -nostartfiles -mhard-float -T openstub.ld PADReadHID.c -o ../../../data/PADReadHID.bin
cd ../../../data
%devpath%\bin\powerpc-eabi-objcopy -S -O binary PADReadGC.bin
%devpath%\bin\powerpc-eabi-objcopy -S -O binary PADReadHID.bin
cd ../source/ppc
