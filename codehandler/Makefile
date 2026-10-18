#---------------------------------------------------------------------------------
.SUFFIXES:
ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif
include $(DEVKITPPC)/wii_rules
#---------------------------------------------------------------------------------
TARGET	:= codehandler.h codehandleronly.h
#---------------------------------------------------------------------------------
# Pick the bundled Windows or Linux helper. See kernel/Makefile for why $(OS)
# alone is not enough here.
UNAME_S := $(shell uname -s 2>/dev/null)
ifneq (,$(filter Windows_NT,$(OS))$(findstring MINGW,$(UNAME_S))$(findstring MSYS,$(UNAME_S))$(findstring CYGWIN,$(UNAME_S)))
	HOST_EXE := .exe
else
	HOST_EXE :=
endif
BIN2H = ../kernel/bin2h/bin2h$(HOST_EXE)
STRIP = $(DEVKITPPC)/bin/powerpc-eabi-strip
#---------------------------------------------------------------------------------
CFLAGS		:= -nostartfiles -nodefaultlibs -Wl,-Ttext,0x80001000
STRIPFLAGS	:= --strip-debug --strip-all --discard-all -F elf32-powerpc
OCFLAGS		:= -I elf32-powerpc -O binary
#---------------------------------------------------------------------------------
.PHONY: all clean

all: $(TARGET)

%.bin : %.s
	@echo " COMPILE     $<"
	@$(CC) $(CFLAGS) -o $@ $<
	@echo " STRIP       $<"
	@$(STRIP) $(STRIPFLAGS) $@
	@echo " OBJCOPY     $<"
	@$(OBJCOPY) $(OCFLAGS) $@
%.h : %.bin
	@echo " BIN2H       $<"
	@$(BIN2H) $<
clean:
	-$(RM) $(TARGET) $(TARGET:.h=.bin)
