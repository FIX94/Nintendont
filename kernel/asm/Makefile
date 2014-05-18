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

BIN2H =	./bin2h.exe

SFILES		:=	$(wildcard *.S)
ELFFILES	:=	$(SFILES:.S=.elf)
BINFILES	:=	$(ELFFILES:.elf=.bin)
HFILES		:=	$(BINFILES:.bin=.h)

DEPDIR = .deps

all: $(HFILES)

%.elf: %.S
	@echo " ASSEMBLE    $<"
	@$(AS) $< -o $@

%.bin: %.elf
	@echo " STRIP       $<"
	@$(STRIP) $< -O binary -o $@

%.h: %.bin
	@echo " BIN2H       $<"
	@$(BIN2H) $<

$(ELFFILES): $(SFILES)

$(BINFILES): $(ELFFILES)

$(HFILES): $(BINFILES)

clean:
	@echo clean ...
	@rm -fr $(HFILES) $(BINFILES) $(ELFFILES)

