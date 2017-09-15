#---------------------------------------------------------------------------------
.SUFFIXES:
ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif
include $(DEVKITPPC)/wii_rules
#---------------------------------------------------------------------------------
TARGET	:= PADReadGC
BUILD	:= ../../data
OUTPUT	:= $(BUILD)/$(TARGET).bin
#---------------------------------------------------------------------------------
CFLAGS	:= -Os -Wall -s -nostartfiles -mhard-float -T openstub.ld
OCFLAGS	:= -S -O binary
#---------------------------------------------------------------------------------
.PHONY: all clean

all: $(OUTPUT)

$(OUTPUT): $(TARGET).c
	@echo " COMPILE     $<"
	@$(CC) $(CFLAGS) $^ -o $@
	@echo " OBJCOPY     $(TARGET).bin"
	@$(OBJCOPY) $(OCFLAGS) $@
	
clean:
	@rm -fr $(BUILD)/$(TARGET).bin
