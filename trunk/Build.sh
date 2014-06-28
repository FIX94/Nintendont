#!/bin/bash

cd kernel/asm

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



cd ../kernel

echo " "

echo "Building Nintendont Kernel (USB)"

echo " "

make usb=1 clean

make usb=1



echo " "

echo "Building Nintendont Kernel (SD)"

echo " "

make clean

make



cd ../loader/source/ppc

echo " "

echo "Building Nintendont HID"

echo " "

sh ./build.sh



cd ../../../loader

echo " "

echo "Building Nintendont Loader"

echo " "

make clean

make

echo " "
