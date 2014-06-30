@echo off
:: Gets the directory that devkitPPC is installed to
setlocal
set devpath=%DEVKITPPC:/=\%\
set devpath=%devpath:~1,1%:%devpath:~2%

echo PadReadGC.c
%devpath%\bin\powerpc-eabi-gcc -O1 -s -nostartfiles -mhard-float -T openstub.ld PADReadGC.c -o ../../data/PADReadGC.bin
cd ../../data
%devpath%\bin\powerpc-eabi-objcopy -S -O binary PADReadGC.bin
cd ../source/ppc
