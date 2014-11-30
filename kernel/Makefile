PREFIX	=	$(DEVKITARM)/bin/arm-none-eabi-
CC	=	$(PREFIX)gcc
AS	=	$(PREFIX)as
LD	=	$(PREFIX)gcc
STRIP =	$(PREFIX)strip

CFLAGS	=	-mbig-endian -fno-unwind-tables -fomit-frame-pointer -O1 -g -Wall -I. -mcpu=arm926ej-s -mthumb \
	-fno-builtin-toupper -fno-builtin-memcpy -fno-builtin-memset  -fno-builtin-memcmp -fno-builtin-malloc -fno-builtin-free \
	-fdata-sections -ffunction-sections -msoft-float -mfloat-abi=soft

ASFLAGS	=	-mbig-endian -mcpu=arm926ej-s
LDFLAGS	=	-nostartfiles -nodefaultlibs -mbig-endian -Wl,--gc-sections,-T,kernel.ld,-Map,kernel.map -n

TARGET	=	kernel.elf
OBJECTS	=	start.o common.o alloc.o GCAM.o JVSIO.o JVSIOMessage.o FST.o DI.o Patch.o PatchTimers.o PatchWidescreen.o ISO.o \
			Stream.o adp.o EXI.o SI.o ff.o HID.o diskio.o Config.o utils_asm.o ES.o NAND.o main.o syscalls.o vsprintf.o string.o \
			SDI.o usb.o usbstorage.o
BINFILE = ../loader/data/kernel.bin

OBJECTS	+= lwbt/bte.o lwbt/btmemb.o lwbt/btmemr.o lwbt/btpbuf.o lwbt/hci.o lwbt/l2cap.o lwbt/physbusif.o BT.o

.PHONY: FORCE

all: $(TARGET)

$(TARGET) : kernel.ld $(OBJECTS)
	@echo  "LD	$@"
	@$(LD) $(LDFLAGS) $(OBJECTS) be/libgcc.a -o $@
	@$(STRIP) -s -g kernel.elf -o $(BINFILE)

%.o : %.s
	@echo  "AS	$@"
	@$(CC) $(CFLAGS) -D_LANGUAGE_ASSEMBLY -c -x assembler-with-cpp -o $@ $<

%.o : %.S
	@echo  "AS	$@"
	@$(CC) $(CFLAGS) -D_LANGUAGE_ASSEMBLY -c -x assembler-with-cpp -o $@ $<

%.o : %.c
	@echo  "CC	$@"
	@$(CC) $(CFLAGS) -c -o $@ $<

%.d: %.c
	@echo  "DEP	$@"
	@set -e; $(CC) -M $(CFLAGS) $< \
		| sed 's?\($*\)\.o[ :]*?\1.o $@ : ?g' > $@; \
		[ -s $@ ] || rm -f $@

%.d: %.S
	@echo	"DEP	$@"
	@touch $@

-include $(OBJECTS:.o=.d)

clean:
	-rm -f *.elf *.o *.bin *.d *.map lwbt/*.d lwbt/*.o
