
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

ifeq ($(windows), 1)
	BIN2H =	../kernel/bin2h/bin2h.exe
else
	BIN2H =	../kernel/bin2h/bin2h
endif

MACHDEP = -mcpu=750 -mno-eabi -mhard-float
#CFLAGS = $(MACHDEP) -Os -Wall -pipe -ffunction-sections -finline-functions-called-once -mno-sdata --combine -fwhole-program -ffreestanding
CFLAGS = $(MACHDEP) -O1 -s -Werror -Wall -pipe -ffunction-sections -finline-functions-called-once -mno-sdata
LDFLAGS = $(MACHDEP) -n -nostartfiles -nostdlib -Wl,-T,openstub.ld -L.
ASFLAGS = -D_LANGUAGE_ASSEMBLY -DHW_RVL -DTINY

TARGET_LINKED = multidol_ldr.elf
TARGET = ../kernel/asm/multidol_ldr.bin

CFILES = utils.c cache.c main.c
#OBJS = crt0.o _all.o
OBJS = crt0.o utils.o cache.o main.o

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

#_all.o: $(CFILES)
#	@echo " COMPILE ALL "
#	@mkdir -p $(DEPDIR)
#	@$(CC) $(CFLAGS) $(DEFINES) -Wp,-MMD,$(DEPDIR)/$(*F).d,-MQ,"$@",-MP -c $(CFILES) -o $@

$(TARGET_LINKED): $(OBJS)
	@echo " LINK        $@"
	@$(CC) -g -o $@ $(LDFLAGS) $(OBJS) $(LIBS)

$(TARGET): $(TARGET_LINKED)
	@echo " OBJCOPY     $@"
	@$(OBJCOPY) -O binary $< $@
	@echo " BIN2H       $@"
	@$(BIN2H) $(TARGET)

clean:
	rm -rf $(TARGET_LINKED) $(TARGET) $(OBJS) $(DEPDIR)
