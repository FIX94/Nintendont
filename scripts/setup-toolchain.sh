#!/usr/bin/env bash
#
# One-time toolchain setup for building Nintendont.
#
# Installs the exact versions README.md pins, side by side with whatever
# devkitPro you already have, so this does NOT disturb other projects:
#
#     $DEVKITPRO/devkitARM-r53-1
#     $DEVKITPRO/devkitPPC-r35-2
#     $DEVKITPRO/libogc-1.8.23
#
# Why pinned versions, and why they are not simply "pacman install":
#
#   devkitPro's own servers only ever serve the *current* release, by policy.
#   Old versions are not retained there for any project. Nintendont has not
#   been updated for the modern toolchain, and the mismatch is not cosmetic:
#
#     * libOGC 1.8.23 needs the C runtime from devkitPPC r35. Against r41+
#       the link fails with `undefined reference to __syscalls`.
#     * Current libogc removed the internals Nintendont calls
#       (__lwp_thread_stopmultitasking, __lwp_thread_closeall), and libogc2
#       (Extrems' fork) removed raw_irq_handler_t. Neither can build it.
#     * devkitPPC r41+ dropped the `bin2o` make macro that loader/Makefile
#       uses to embed background.png, font.zip and kernel.zip.
#
#   README.md links a Mediafire folder for the archives. This script uses the
#   community archive mirror instead: same bytes (verified byte-identical,
#   SHA-256 below), but direct and scriptable.
#
# Safe to re-run; every step is skipped if already done.
set -euo pipefail

MIRROR="https://wii.leseratte10.de/devkitPro"

# ---------------------------------------------------------------------------
# Host detection
# ---------------------------------------------------------------------------
case "$(uname -s)" in
	MINGW*|MSYS*|CYGWIN*) HOST=windows ;;
	Darwin)               HOST=osx ;;
	Linux)                HOST=linux ;;
	*) echo "Unsupported host: $(uname -s)" >&2; exit 1 ;;
esac

: "${DEVKITPRO:=/opt/devkitpro}"
if [ "$HOST" = windows ] && [ ! -d "$DEVKITPRO" ]; then
	DEVKITPRO=/c/devkitPro
fi

if [ ! -d "$DEVKITPRO" ]; then
	cat >&2 <<EOF
devkitPro not found at: $DEVKITPRO

Install it first (it provides the MSYS2 shell, make, and the host tools in
tools/bin that the build needs), then re-run this script:
  https://devkitpro.org/wiki/Getting_Started
Set DEVKITPRO if you installed it somewhere non-standard.
EOF
	exit 1
fi

log()  { printf '\n==> %s\n' "$1"; }
warn() { printf '\n!!! %s\n' "$1" >&2; }

# ---------------------------------------------------------------------------
# Pinned packages
#
# SHA-256 values below were verified on 2026-09-21 by downloading each file
# from BOTH this mirror and the Mediafire folder README.md links, and
# confirming the two copies are byte-identical. All three were scanned clean
# by MetaDefender Cloud (19 engines + sandbox) and by ClamAV 1.5.3.
#
# Only the Windows packages have been hash-verified by us so far. The Linux
# and macOS entries are left unpinned deliberately rather than guessed -- the
# script prints the hash it got so you can check it and send a patch adding it.
# ---------------------------------------------------------------------------
sha_for() {
	case "$1" in
		devkitARM-r53-1-windows.pkg.tar.xz) echo 61190560711e773e517afa019a7ba4333fd74162dab70819431af4d7f87ab35f ;;
		devkitPPC-r35-2-windows.pkg.tar.xz) echo 7d13a2b8904a9a1e26e2f59cda1471dec3d7e5ead673668fd588208e3033cbf4 ;;
		libogc-1.8.23-1-any.pkg.tar.xz)     echo e5b61a2e7589ab95e321cae30bbf1511fbe4729f3120e782201eebf8514c6707 ;;
		*) echo "" ;;
	esac
}

sha256_of() {
	if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | cut -d' ' -f1
	else shasum -a 256 "$1" | cut -d' ' -f1; fi
}

# install_pkg <dest-dir> <name-inside-package> <url-path>
install_pkg() {
	local dest="$1" inner="$2" urlpath="$3"
	local file; file="$(basename "$urlpath")"

	if [ -d "$dest" ]; then
		log "$(basename "$dest") already installed, skipping"
		return 0
	fi

	log "Downloading $file"
	local tmp; tmp="$(mktemp -d)"
	trap 'rm -rf "$tmp"' RETURN
	curl -fL --progress-bar --max-time 900 -o "$tmp/$file" "$MIRROR/$urlpath"

	local want got
	want="$(sha_for "$file")"
	got="$(sha256_of "$tmp/$file")"
	if [ -n "$want" ]; then
		if [ "$want" != "$got" ]; then
			warn "SHA-256 MISMATCH for $file
  expected $want
  got      $got
Refusing to install. Do not use this download."
			return 1
		fi
		echo "    sha256 ok ($got)"
	else
		warn "No pinned SHA-256 for $file (this host's package).
  got $got
  Please verify it independently, then add it to sha_for() and send a patch."
	fi

	tar -xf "$tmp/$file" -C "$tmp"
	# devkitPro packages unpack to opt/devkitpro/<name>.
	mv "$tmp/opt/devkitpro/$inner" "$dest"
	echo "    installed -> $dest"
}

install_pkg "$DEVKITPRO/devkitARM-r53-1" devkitARM \
	"devkitARM/r53%20%282019-06%29/devkitARM-r53-1-$HOST.pkg.tar.xz"

install_pkg "$DEVKITPRO/devkitPPC-r35-2" devkitPPC \
	"devkitPPC/r35/devkitPPC-r35-2-$HOST.pkg.tar.xz"

install_pkg "$DEVKITPRO/libogc-1.8.23" libogc \
	"libogc/libogc_1.8.23%20%282019-10-02%29/libogc-1.8.23-1-any.pkg.tar.xz"

# ---------------------------------------------------------------------------
# devkitPPC make rules
#
# The r35-2 package contains only the compiler -- no base_rules/wii_rules
# (in that era they shipped in a separate devkitppc-rules package). The build
# needs them, and wii_rules is also where LIBOGC_INC/LIBOGC_LIB are set, so we
# write a minimal set here pointing at the pinned libogc. Self-contained, and
# it cannot be disturbed by whatever the system devkitPPC does.
# ---------------------------------------------------------------------------
RULES_DIR="$DEVKITPRO/devkitPPC-r35-2"
if [ -f "$RULES_DIR/wii_rules" ] && grep -q "libogc-1.8.23" "$RULES_DIR/wii_rules"; then
	log "devkitPPC-r35-2 make rules already in place"
else
	log "Writing devkitPPC-r35-2 make rules (pointing at the pinned libogc)"

	cat > "$RULES_DIR/base_tools" <<'EOF'
ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment.")
endif

export PATH := $(DEVKITPRO)/tools/bin:$(DEVKITPPC)/bin:$(PATH)
export PORTLIBS_PATH := $(DEVKITPRO)/portlibs

PREFIX ?= powerpc-eabi-

export AS  := $(PREFIX)as
export CC  := $(PREFIX)gcc
export CXX := $(PREFIX)g++
export AR  := $(PREFIX)gcc-ar
export OBJCOPY := $(PREFIX)objcopy
export STRIP   := $(PREFIX)strip
export NM      := $(PREFIX)gcc-nm
export RANLIB  := $(PREFIX)gcc-ranlib
EOF

	cat > "$RULES_DIR/base_rules" <<'EOF'
include $(DEVKITPPC)/base_tools

%.a:
	@echo $(notdir $@)
	@rm -f $@
	@$(AR) -rc $@ $^

%.o: %.cpp
	@echo $(notdir $<)
	@$(CXX) -MMD -MP -MF $(DEPSDIR)/$*.d $(CXXFLAGS) -c $< -o $@ $(ERROR_FILTER)

%.o: %.c
	@echo $(notdir $<)
	@$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(CFLAGS) -c $< -o $@ $(ERROR_FILTER)

%.o: %.S
	@echo $(notdir $<)
	@$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d -x assembler-with-cpp $(ASFLAGS) -c $< -o $@ $(ERROR_FILTER)

# Embed a binary file as a linkable object plus a header declaring it.
# loader/Makefile relies on this; devkitPPC r41+ removed it.
define bin2o
	bin2s -a 32 $< | $(AS) -o $(@)
	echo "extern const u8" `(echo $(<F) | sed -e 's/^\([0-9]\)/_\1/' -e 's/[^A-Za-z0-9_]/_/g')`"_end[];" > `(echo $(<F) | tr . _)`.h
	echo "extern const u8" `(echo $(<F) | sed -e 's/^\([0-9]\)/_\1/' -e 's/[^A-Za-z0-9_]/_/g')`"[];" >> `(echo $(<F) | tr . _)`.h
	echo "extern const u32" `(echo $(<F) | sed -e 's/^\([0-9]\)/_\1/' -e 's/[^A-Za-z0-9_]/_/g')`_size";" >> `(echo $(<F) | tr . _)`.h
endef
EOF

	cat > "$RULES_DIR/wii_rules" <<'EOF'
include $(DEVKITPPC)/base_rules

PORTLIBS := $(PORTLIBS_PATH)/wii $(PORTLIBS_PATH)/ppc

# Pinned libOGC 1.8.23-1 -- see scripts/setup-toolchain.sh for why.
export LIBOGC_INC := $(DEVKITPRO)/libogc-1.8.23/include
export LIBOGC_LIB := $(DEVKITPRO)/libogc-1.8.23/lib/wii

MACHDEP = -DGEKKO -mrvl -mcpu=750 -meabi -mhard-float

%.dol: %.elf
	@echo output ... $(notdir $@)
	@elf2dol $< $@

%.elf:
	@echo linking ... $(notdir $@)
	@$(LD) $^ $(LDFLAGS) $(LIBPATHS) $(LIBS) -o $@
EOF
fi

# ---------------------------------------------------------------------------
# Verify
# ---------------------------------------------------------------------------
log "Verifying"
fail=0
check() { if [ -e "$2" ]; then printf '  %-42s ok\n' "$1"; else printf '  %-42s MISSING\n' "$1"; fail=1; fi; }

EXE=""; [ "$HOST" = windows ] && EXE=".exe"
check "devkitPPC r35-2 compiler"  "$DEVKITPRO/devkitPPC-r35-2/bin/powerpc-eabi-gcc$EXE"
check "devkitARM r53-1 compiler"  "$DEVKITPRO/devkitARM-r53-1/bin/arm-none-eabi-gcc$EXE"
check "libOGC 1.8.23 headers"     "$DEVKITPRO/libogc-1.8.23/include/ogc/lwp_threads.h"
check "libOGC 1.8.23 library"     "$DEVKITPRO/libogc-1.8.23/lib/wii/libogc.a"
check "devkitPPC r35-2 wii_rules" "$DEVKITPRO/devkitPPC-r35-2/wii_rules"
check "elf2dol (devkitPro tools)" "$DEVKITPRO/tools/bin/elf2dol$EXE"

# The three libogc internals Nintendont depends on, which newer libogc removed.
for sym in __lwp_thread_stopmultitasking __lwp_thread_closeall raw_irq_handler_t; do
	if grep -rqs "$sym" "$DEVKITPRO/libogc-1.8.23/include/"; then
		printf '  %-42s ok\n' "libogc symbol $sym"
	else
		printf '  %-42s MISSING\n' "libogc symbol $sym"; fail=1
	fi
done

[ "$fail" = 0 ] || { warn "Setup incomplete."; exit 1; }

cat <<EOF

Toolchain ready. Build with:

  scripts/build.sh

EOF
