#!/bin/sh

cd multidol
echo " "
echo "Building Multi-DOL Loader"
echo " "
make clean
make

cd ../kernel/asm
echo " "
echo "Building asm files"
echo " "
make clean
make

cd ../../resetstub
echo " "
echo "Building Reset Stub"
echo " "
make clean
make

cd ../fatfs
echo " "
echo "Building FatFS libraries"
echo " "
make -f Makefile.arm clean
make -f Makefile.arm
make -f Makefile.ppc clean
make -f Makefile.ppc

cd ../codehandler
echo " "
echo "Building Nintendont Codehandler"
echo " "
sh ./build.sh

cd ../kernel
echo " "
echo "Building Nintendont Kernel"
echo " "
make clean
make

cd ../loader/source/ppc/IOSInterface
echo " "
echo "Building Nintendont IOS Interface"
echo " "
sh ./build.sh

cd ../PADRead
echo " "
echo "Building Nintendont HID"
echo " "
sh ./build.sh

cd ../../../../loader
echo " "
echo "Building Nintendont Loader"
echo " "
make clean
make

echo " "
