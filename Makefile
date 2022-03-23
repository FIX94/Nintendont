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
# This is where titles.txt and icon.png will be copied to
ARTIFACTS := build/app/Nintendont
NINTENTDONTVERSION_H := common/include/NintendontVersion.h
.PHONY: all forced clean $(SUBPROJECTS)

all: nintendontversion loader copy_files
forced: clean all

# change version number
nintendontversion:
	@echo " "
	@echo "Update version number (if available)"
	@echo " "
	if [ -n "$(GITHUB_RUN_NUMBER)" ]; then \
		sed -i 's/#define NIN_MINOR_VERSION.*$$/#define NIN_MINOR_VERSION\t\t\t$(GITHUB_RUN_NUMBER)/' $(NINTENTDONTVERSION_H); \
	fi

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

# After building, assemble a build directory. GitHub will zip it and it can be
# downloaded and extracted to a SD card or HDD.
copy_files:
	@echo " "
	@echo "Copying files"
	@echo " "
	mkdir -p $(ARTIFACTS)
	cp -p nintendont/icon.png nintendont/titles.txt $(ARTIFACTS)
	unzip -d build/controllers -q -n controllerconfigs/controllers.zip

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
