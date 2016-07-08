@echo off
:: Gets the directory that devkitPPC is installed to
setlocal
set devpath=%DEVKITPPC:/=\%\
set devpath=%devpath:~1,1%:%devpath:~2%

echo codehandler.s
%devpath%\bin\powerpc-eabi-gcc -nostartfiles -nodefaultlibs -Wl,-Ttext,0x93006000 -o ../kernel/bin2h/codehandler.bin codehandler.s
cd ../kernel/bin2h/
%devpath%\bin\powerpc-eabi-strip --strip-debug --strip-all --discard-all -F elf32-powerpc codehandler.bin
%devpath%\bin\powerpc-eabi-objcopy -I elf32-powerpc -O binary codehandler.bin
bin2h codehandler.bin
del /f codehandler.bin
move codehandler.h ..
