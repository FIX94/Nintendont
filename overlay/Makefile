PREFIX := $(DEVKITPPC)/bin/powerpc-eabi-
CFLAGS := -Os -mcpu=750 -m32 -mbig-endian -msoft-float -msdata=none -ffreestanding -fno-builtin -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables -Wall -Wextra

# Under -Os, GCC 8 restores callee-saved registers through out-of-line helpers
# (_restgpr_N_x) that live in libgcc's crtresxgpr.o - 212 bytes of pure
# register restores, the reference implementation the compiler's own epilogue
# is written against. Runtime division by a variable also needs libgcc's
# __udivsi3/__umodsi3 (pure functions, no memory, no globals - safe in the VI
# interrupt). Handing ld the archive pulls in only the members that are
# referenced; the `check` step proves after every link that nothing else did.
LIBGCC := $(shell $(PREFIX)gcc -mcpu=750 -print-libgcc-file-name)
ifeq ($(OS),Windows_NT)
BIN2H := ../kernel/bin2h/bin2h.exe
else
BIN2H := ../kernel/bin2h/bin2h
endif

# Symbols the module is allowed to take from libgcc. Anything else linked in
# that our own two objects did not define is a build failure.
ALLOWED_LIBGCC := ^(_(save|rest)gpr_[0-9]+(_x)?|__u?(div|mod)si3|__u?divmodsi4)$$

all: overlay.h check
menu.o: menu.c ../common/include/Overlay.h
	$(PREFIX)gcc $(CFLAGS) -c menu.c -o $@
entry.o: entry.S
	$(PREFIX)as -mregnames entry.S -o $@
overlay.elf: menu.o entry.o link.ld
	$(PREFIX)ld -T link.ld entry.o menu.o $(LIBGCC) -o $@
overlay.bin: overlay.elf
	$(PREFIX)objcopy -O binary $< $@
overlay.h: overlay.bin
	$(BIN2H) overlay.bin

# Derived mechanically, not from a hand-kept list: every symbol the ELF
# defines must come from menu.o/entry.o, or be an explicitly accepted libgcc
# helper. Robust to GCC renaming statics (.constprop, .isra, .part).
check: overlay.elf overlay.bin
	@$(PREFIX)nm menu.o entry.o | awk '$$2!="U"{print $$3}' | sort -u > .ours.txt
	@$(PREFIX)nm overlay.elf | awk '$$2!="U"{print $$3}' | sort -u > .elf.txt
	@comm -13 .ours.txt .elf.txt | grep -vE '$(ALLOWED_LIBGCC)' | grep -vE '^(_(s|e)?(text|data|bss)|__bss_(start|end)|_(etext|edata|end))$$' \
	  | ( ! grep . ) || { echo "overlay: symbol linked in that is neither ours nor an accepted libgcc helper:"; comm -13 .ours.txt .elf.txt; exit 1; }
	@rm -f .ours.txt .elf.txt
	@$(PREFIX)nm overlay.elf | awk '$$2=="U"' | ( ! grep . ) || { echo "overlay: unresolved symbols remain"; exit 1; }
	@$(PREFIX)size overlay.elf
	@test $$(stat -c%s overlay.bin) -le 49152 || { echo "overlay.bin exceeds 0xC000"; exit 1; }

.PHONY: all check clean
clean:
	rm -f menu.o entry.o overlay.elf overlay.bin overlay.h .ours.txt .elf.txt
