#PREFIX = powerpc-gekko-
PREFIX = $(DEVKITPPC)/bin/powerpc-eabi-
#PREFIX = /home/megazig/Wii/bootmii-utils/bin/powerpc-elf-

AR = $(PREFIX)ar
AS = $(PREFIX)as
CC = $(PREFIX)gcc
CXX = $(PREFIX)g++
LD = $(PREFIX)ld
OBJCOPY = $(PREFIX)objcopy
RANLIB = $(PREFIX)ranlib
STRIP = $(PREFIX)strip

# Pick the bundled Windows or Linux helper. See kernel/Makefile for why $(OS)
# alone is not enough here.
UNAME_S := $(shell uname -s 2>/dev/null)
ifneq (,$(filter Windows_NT,$(OS))$(findstring MINGW,$(UNAME_S))$(findstring MSYS,$(UNAME_S))$(findstring CYGWIN,$(UNAME_S)))
	HOST_EXE := .exe
else
	HOST_EXE :=
endif
BIN2H = ../bin2h/bin2h$(HOST_EXE)

SFILES		:=	$(wildcard *.S)
DFILES		:=	$(SFILES:.S=.d)
ELFFILES	:=	$(SFILES:.S=.elf)
BINFILES	:=	$(ELFFILES:.elf=.bin)
HFILES		:=	$(BINFILES:.bin=.h)

all: $(HFILES)

-include $(DFILES)
.PRECIOUS: %.elf %.bin

%.elf: %.S
	@echo " ASSEMBLE    $<"
	@$(AS) $< -mregnames -MD $(@:.elf=.d) -o $@

%.bin: %.elf
	@echo " STRIP       $<"
	@$(STRIP) $< -O binary -S -s -o $@

%.h: %.bin
	@echo " BIN2H       $<"
	@$(BIN2H) $<

clean:
	@echo clean ...
	@rm -fr *.d *.h *.bin *.elf
