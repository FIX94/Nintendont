#---------------------------------------------------------------------------------
# Clear the implicit built in rules
#---------------------------------------------------------------------------------

.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif
ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

SUBPROJECTS := multidol kernel/asm resetstub \
	fatfs/libfat-arm.a fatfs/libfat-ppc.a \
	codehandler kernel kernelboot \
	loader/source/ppc loader
.PHONY: all forced clean $(SUBPROJECTS)

all: loader
forced: clean all

# Force MacOS/Linux users to compile their own copy of bin2h
bin2h: bin2h
	@echo " "
	@echo "Building bin2h"
	@echo " "
	$(MAKE) -C kernel/bin2h

multidol:
	@echo " "
	@echo "Building Multi-DOL loader"
	@echo " "
	$(MAKE) -C multidol

kernel/asm: bin2h
	@echo " "
	@echo "Building asm files"
	@echo " "
	$(MAKE) -C kernel/asm

resetstub:
	@echo " "
	@echo "Building reset stub"
	@echo " "
	$(MAKE) -C resetstub

fatfs/libfat-arm.a:
	@echo " "
	@echo "Building FatFS library for ARM"
	@echo " "
	$(MAKE) -C fatfs -f Makefile.arm

fatfs/libfat-ppc.a:
	@echo " "
	@echo "Building FatFS library for PPC"
	@echo " "
	$(MAKE) -C fatfs -f Makefile.ppc

codehandler: bin2h
	@echo " "
	@echo "Building Nintendont code handler"
	@echo " "
	$(MAKE) -C codehandler

kernel: kernel/asm fatfs/libfat-arm.a codehandler bin2h
	@echo " "
	@echo "Building Nintendont kernel"
	@echo " "
	$(MAKE) -C kernel

loader/source/ppc:
	@echo " "
	@echo "Building Nintendont HID"
	@echo " "
	$(MAKE) -C loader/source/ppc

kernelboot:
	@echo " "
	@echo "Building Nintendont kernelboot"
	@echo " "
	$(MAKE) -C kernelboot

loader: multidol resetstub fatfs/libfat-ppc.a kernel kernelboot loader/source/ppc
	@echo " "
	@echo "Building Nintendont loader"
	@echo " "
	$(MAKE) -C loader

clean:
	@echo " "
	@echo "Cleaning all subprojects..."
	@echo " "
	$(MAKE) -C multidol clean
	$(MAKE) -C kernel/asm clean
	$(MAKE) -C resetstub clean
	$(MAKE) -C fatfs -f Makefile.arm clean
	$(MAKE) -C fatfs -f Makefile.ppc clean
	$(MAKE) -C codehandler clean
	$(MAKE) -C kernel clean
	$(MAKE) -C kernelboot clean
	$(MAKE) -C loader/source/ppc clean
	$(MAKE) -C loader clean
