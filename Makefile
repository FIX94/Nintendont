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
	loader/source/ppc/PADReadGC loader/source/ppc/IOSInterface loader
.PHONY: all forced clean $(SUBPROJECTS)

all: loader
forced: clean all

multidol:
	@echo " "
	@echo "Building Multi-DOL loader"
	@echo " "
	$(MAKE) -C multidol

kernel/asm:
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

codehandler:
	@echo " "
	@echo "Building Nintendont code handler"
	@echo " "
	$(MAKE) -C codehandler

kernel: kernel/asm fatfs/libfat-arm.a codehandler
	@echo " "
	@echo "Building Nintendont kernel"
	@echo " "
	$(MAKE) -C kernel

loader/source/ppc/PADReadGC:
	@echo " "
	@echo "Building Nintendont PADReadGC"
	@echo " "
	$(MAKE) -C loader/source/ppc/PADReadGC

loader/source/ppc/IOSInterface:
	@echo " "
	@echo "Building Nintendont IOSInterface"
	@echo " "
	$(MAKE) -C loader/source/ppc/IOSInterface

kernelboot:
	@echo " "
	@echo "Building Nintendont kernelboot"
	@echo " "
	$(MAKE) -C kernelboot

loader: multidol resetstub fatfs/libfat-ppc.a kernel kernelboot loader/source/ppc/PADReadGC loader/source/ppc/IOSInterface
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
	$(MAKE) -C loader/source/ppc/PADReadGC clean
	$(MAKE) -C loader/source/ppc/IOSInterface clean
	$(MAKE) -C loader clean
