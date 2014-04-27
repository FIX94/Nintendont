@echo off
for /R %%i IN (*.S) DO %devkitpro:~1,1%:\%devkitpro:~3%\devkitPPC\bin\powerpc-eabi-as.exe %%i -o %%~ni.elf
for /R %%i IN (*.S) DO %devkitpro:~1,1%:\%devkitpro:~3%\devkitPPC\bin\powerpc-eabi-strip.exe -s %%~ni.elf -O binary -o %%~ni.bin
for /R %%i in (*.bin) DO bin2h.exe %%~ni.bin
