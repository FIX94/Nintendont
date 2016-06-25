# Nintendont kernel

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

ifeq ($(windows), 1)
	ZIP = zip/zip.exe
else
	ZIP = zip
endif

CFLAGS	:= -mbig-endian -O3 -std=gnu89 -g -I. -Wall \
	   -mcpu=arm926ej-s -msoft-float -mfloat-abi=soft \
	   -fdata-sections -ffunction-sections -fno-builtin-toupper -fno-builtin-memcpy \
	   -fno-builtin-memset -fno-builtin-memcmp -fno-builtin-malloc -fno-builtin-free
CPPFLAGS := -I. -I../fatfs -I../codehandler

ASFLAGS	:= -mbig-endian -mcpu=arm926ej-s
LDFLAGS	:= -nostartfiles -nodefaultlibs -mbig-endian -Wl,--gc-sections,-T,kernel.ld,-Map,kernel.map -n

TARGET	:= kernel.elf
OBJECTS	:= start.o common.o alloc.o GCAM.o JVSIO.o JVSIOMessage.o FST.o DI.o RealDI.o \
	   Patch.o PatchTimers.o TRI.o PatchWidescreen.o ISO.o Stream.o adp.o \
	   EXI.o SRAM.o GCNCard.o SI.o HID.o diskio.o Config.o utils_asm.o ES.o NAND.o \
	   main.o syscalls.o ReadSpeed.o vsprintf.o string.o prs.o \
	   SDI.o usb.o usbstorage.o wdvd.o
LIBS	:= ../fatfs/libfatfs-arm.a be/libc.a be/libgcc.a
ZIPFILE	:= ../loader/data/kernel.zip

# Bluetooth stack
OBJECTS	+= lwbt/bte.o lwbt/btmemb.o lwbt/btmemr.o lwbt/btpbuf.o lwbt/hci.o lwbt/l2cap.o lwbt/physbusif.o BT.o

.PHONY: FORCE

all: $(TARGET)

-include $(OBJECTS:.o=.d)

$(TARGET): kernel.ld $(OBJECTS)
	@echo  "LD	$@"
	@$(LD) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $@
	@$(OBJCOPY) -S -O binary kernel.elf kernel.bin
	@echo  "ZIP	$(ZIPFILE)"
	@$(ZIP) -JXjq9 $(ZIPFILE) kernel.bin

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
	-$(RM) -f $(ZIPFILE) *.elf *.o *.bin *.d *.map lwbt/*.d lwbt/*.o option/*.d option/*.o
