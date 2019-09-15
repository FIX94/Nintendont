#---------------------------------------------------------------------------------
.SUFFIXES:
ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif
include $(DEVKITPPC)/wii_rules
#---------------------------------------------------------------------------------
TARGET	:= IOSInterface
BUILD	:= ../../../data
OUTELF	:= $(CURDIR)/IOSInterface.elf
OUTPUT	:= $(BUILD)/$(TARGET).bin
#---------------------------------------------------------------------------------
CFLAGS	:= -O1 -Wall -s -nostartfiles -nostdlib -mhard-float
ASFLAGS := $(CFLAGS)
LDFLAGS := -T link.ld
OCFLAGS	:= -S -O binary
#---------------------------------------------------------------------------------
CFILES		:=	$(foreach dir,$(CURDIR),$(notdir $(wildcard $(dir)/*.c)))
SFILES		:=	$(foreach dir,$(CURDIR),$(notdir $(wildcard $(dir)/*.S)))
OFILES		:=	$(CFILES:.c=.o) $(SFILES:.S=.o)

export LD	:=	$(CC)

.PHONY: all clean

all: $(OUTPUT)

$(OUTELF): $(OFILES)

$(OUTPUT): $(OUTELF)
	@echo "output ... $(TARGET).bin"
	@$(OBJCOPY) $(OCFLAGS) $^ $@

clean:
	@rm -fr $(OFILES) $(OUTELF) $(OUTPUT)
