@echo off

cd multidol
echo.
echo Building Multi-DOL Loader
echo.
make windows=1 clean
make windows=1

cd ..\kernel\asm
echo.
echo Building asm files
echo.
make windows=1 clean
make windows=1

cd ..\..\resetstub
echo.
echo Building Reset Stub
echo.
make clean
make

cd ..\kernel
echo.
echo Building Nintendont Kernel
echo.
make clean
make

cd ..\loader\source\ppc
echo.
echo Building Nintendont HID
echo.
call build.bat

cd ..\..\..\loader
echo.
echo Building Nintendont Loader
echo.
make clean
make

echo.
pause