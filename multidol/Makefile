
PREFIX = $(DEVKITPPC)/bin/powerpc-eabi-

AR = $(PREFIX)ar
AS = $(PREFIX)as
CC = $(PREFIX)gcc
CXX = $(PREFIX)g++
LD = $(PREFIX)ld
OBJCOPY = $(PREFIX)objcopy
RANLIB = $(PREFIX)ranlib
STRIP = $(PREFIX)strip

MACHDEP = -mcpu=750 -meabi -mhard-float
CFLAGS = $(MACHDEP) -O1 -s -Werror -Wall -fdata-sections -ffunction-sections
LDFLAGS = $(MACHDEP) -n -nostartfiles -nostdlib -Wl,--gc-sections,-T,openstub.ld -L.
ASFLAGS = -D_LANGUAGE_ASSEMBLY -DHW_RVL

TARGET_LINKED = multidol_ldr.elf
TARGET = ../loader/data/multidol_ldr.bin

CFILES = cache.c main.c global.c apploader.c dip.c utils.c
OBJS = crt0.o cache.o main.o global.o apploader.o dip.o utils.o

DEPDIR = .deps

LIBS = 

all: $(TARGET)

%.o: %.s
	@echo " ASSEMBLE    $<"
	@$(CC) $(CFLAGS) $(DEFINES) $(ASFLAGS) -c $< -o $@

%.o: %.S
	@echo " ASSEMBLE    $<"
	@$(CC) $(CFLAGS) $(DEFINES) $(ASFLAGS) -c $< -o $@

%.o: %.c
	@echo " COMPILE     $<"
	@$(CC) $(CFLAGS) $(DEFINES) -c $< -o $@

$(TARGET_LINKED): $(OBJS)
	@echo " LINK        $@"
	@$(CC) -g -o $@ $(LDFLAGS) $(OBJS) $(LIBS)

$(TARGET): $(TARGET_LINKED)
	@echo " OBJCOPY     $@"
	@$(OBJCOPY) -O binary -S $< $@

clean:
	-$(RM) -rf $(TARGET_LINKED) $(TARGET) $(OBJS) $(DEPDIR)
