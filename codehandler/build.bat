@echo off
:: Gets the directory that devkitPPC is installed to
setlocal
set devpath=%DEVKITPPC:/=\%\
set devpath=%devpath:~1,1%:%devpath:~2%

echo  CC          codehandler.s
%devpath%\bin\powerpc-eabi-gcc -nostartfiles -nodefaultlibs -Wl,-Ttext,0x80001000 -o codehandler.bin codehandler.s
echo  CC          codehandleronly.s
%devpath%\bin\powerpc-eabi-gcc -nostartfiles -nodefaultlibs -Wl,-Ttext,0x80001000 -o codehandleronly.bin codehandleronly.s
echo  STRIP       codehandler.bin
%devpath%\bin\powerpc-eabi-strip --strip-debug --strip-all --discard-all -F elf32-powerpc codehandler.bin
echo  STRIP       codehandleronly.bin
%devpath%\bin\powerpc-eabi-strip --strip-debug --strip-all --discard-all -F elf32-powerpc codehandleronly.bin
echo  OBJCOPY     codehandler.bin
%devpath%\bin\powerpc-eabi-objcopy -I elf32-powerpc -O binary codehandler.bin
echo  OBJCOPY     codehandleronly.bin
%devpath%\bin\powerpc-eabi-objcopy -I elf32-powerpc -O binary codehandleronly.bin
echo  BIN2H       codehandler.h
..\kernel\bin2h\bin2h.exe codehandler.bin
echo  BIN2H       codehandleronly.h
..\kernel\bin2h\bin2h.exe codehandleronly.bin
echo  RM          codehandler.bin codehandleronly.bin
del /f codehandler.bin codehandleronly.bin
