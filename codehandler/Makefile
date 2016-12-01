#---------------------------------------------------------------------------------
.SUFFIXES:
ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif
include $(DEVKITPPC)/wii_rules
#---------------------------------------------------------------------------------
TARGET	:= codehandler.h codehandleronly.h
#---------------------------------------------------------------------------------
ifeq ($(OS),Windows_NT)
	BIN2H =	../kernel/bin2h/bin2h.exe
else
	BIN2H =	../kernel/bin2h/bin2h
endif
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
