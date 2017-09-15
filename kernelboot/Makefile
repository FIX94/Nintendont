# Nintendont kernelboot

#---------------------------------------------------------------------------------
# Clear the implicit built in rules
#---------------------------------------------------------------------------------

.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

PREFIX	:= $(DEVKITARM)/bin/arm-none-eabi-
CC	:= $(PREFIX)gcc
AS	:= $(PREFIX)as
LD	:= $(PREFIX)gcc
OBJCOPY	:= $(PREFIX)objcopy

CFLAGS	:= -mbig-endian -Os -fPIC -std=gnu89 -g -I. -Wall \
	   -mcpu=arm926ej-s -msoft-float -mfloat-abi=soft \
	   -fdata-sections -ffunction-sections -fno-builtin-toupper -fno-builtin-memcpy \
	   -fno-builtin-memset -fno-builtin-memcmp -fno-builtin-malloc -fno-builtin-free
CPPFLAGS := -I. -I../fatfs -I../codehandler

ASFLAGS	:= -mbig-endian -mcpu=arm926ej-s -fPIC
LDFLAGS	:= -nostartfiles -nodefaultlibs -mbig-endian -fPIC -Wl,--gc-sections -n

BUILD	:= ../loader/data
TARGET := kernelboot
OUTPUT := $(TARGET).elf
BINFILE := $(BUILD)/$(TARGET).bin
OBJECTS	:= _start.o main.o syscalls.o

.PHONY: FORCE

all: $(OUTPUT)

-include $(OBJECTS:.o=.d)

$(OUTPUT): $(OBJECTS)
	@echo  "LD	$@"
	@$(LD) $(LDFLAGS) $(OBJECTS) -o $@
	@$(OBJCOPY) -S -O binary $@ $(BINFILE)

%.o: %.s
	@echo  "AS	$@"
	@$(CC) $(CFLAGS) $(CPPFLAGS) -D_LANGUAGE_ASSEMBLY -c -MMD -MP -MF $(@:.o=.d) -x assembler-with-cpp -o $@ $<

%.o: %.S
	@echo  "AS	$@"
	@$(CC) $(CFLAGS) $(CPPFLAGS) -D_LANGUAGE_ASSEMBLY -c -MMD -MP -MF $(@:.o=.d) -x assembler-with-cpp -o $@ $<

%.o : %.c
	@echo  "CC	$@"
	@$(CC) $(CFLAGS) $(CPPFLAGS) -c -MMD -MP -MF $(@:.o=.d) -o $@ $<

clean:
	-$(RM) -f $(ZIPFILE) *.elf *.o *.bin *.d
