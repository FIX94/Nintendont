@echo off
:: Gets the directory that devkitPPC is installed to
setlocal
::set devpath=%DEVKITPPC:/=\%\
::set devpath=%devpath:~1,1%:%devpath:~2%
set devpath=C:\devkitpro\devkitPPC
echo codehandler.s
%devpath%\bin\powerpc-eabi-gcc -nostartfiles -nodefaultlibs -Wl,-Ttext,0x80001000 -o ../kernel/bin2h/codehandler.bin codehandler.s
echo codehandleronly.s
%devpath%\bin\powerpc-eabi-gcc -nostartfiles -nodefaultlibs -Wl,-Ttext,0x80001000 -o ../kernel/bin2h/codehandleronly.bin codehandleronly.s
cd ../kernel/bin2h
%devpath%\bin\powerpc-eabi-strip --strip-debug --strip-all --discard-all -F elf32-powerpc codehandler.bin
%devpath%\bin\powerpc-eabi-objcopy -I elf32-powerpc -O binary codehandler.bin
%devpath%\bin\powerpc-eabi-strip --strip-debug --strip-all --discard-all -F elf32-powerpc codehandleronly.bin
%devpath%\bin\powerpc-eabi-objcopy -I elf32-powerpc -O binary codehandleronly.bin
bin2h codehandler.bin
bin2h codehandleronly.bin
del /f codehandler.bin codehandleronly.bin
move /Y codehandler.h .. >nul
move /Y codehandleronly.h .. >nul
