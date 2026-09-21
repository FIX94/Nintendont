#!/usr/bin/env bash
#
# Build Nintendont with the pinned toolchain.
# Run scripts/setup-toolchain.sh once first.
#
# Usage:
#   scripts/build.sh            # everything -> loader/loader.dol
#   scripts/build.sh kernel     # ARM kernel only -> loader/data/kernel.zip
#   scripts/build.sh loader     # PPC loader only (kernel.zip must exist)
#   scripts/build.sh clean
#
# Any extra arguments are passed straight to make, e.g.
#   scripts/build.sh all -j8
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

case "$(uname -s)" in
	MINGW*|MSYS*|CYGWIN*) HOST=windows ;;
	Darwin)               HOST=osx ;;
	*)                    HOST=linux ;;
esac

# --- Toolchain -------------------------------------------------------------
# Assigned, not defaulted: a stale DEVKITPRO in the environment pointing at a
# path that does not exist on this machine is a common and confusing failure.
if [ -z "${DEVKITPRO:-}" ] || [ ! -d "${DEVKITPRO:-}" ]; then
	if [ "$HOST" = windows ] && [ -d /c/devkitPro ]; then DEVKITPRO=/c/devkitPro
	else DEVKITPRO=/opt/devkitpro; fi
fi
export DEVKITPRO

# The pinned installs win over anything inherited. This is deliberate and it
# matters: devkitPro's installer exports DEVKITPPC/DEVKITARM globally, so a
# `${DEVKITPPC:-pinned}` default would silently keep the *current* toolchain
# and build against it. The failure then looks like broken source -- e.g.
# multidol/apploader.c stopping on -Wincompatible-pointer-types, which is a
# warning on the pinned gcc 8.3 and a hard error on gcc 14+.
# Set NINTENDONT_DEVKITPPC / NINTENDONT_DEVKITARM to override on purpose.
DEVKITPPC="${NINTENDONT_DEVKITPPC:-$DEVKITPRO/devkitPPC-r35-2}"
DEVKITARM="${NINTENDONT_DEVKITARM:-$DEVKITPRO/devkitARM-r53-1}"
[ -d "$DEVKITPPC" ] || DEVKITPPC="$DEVKITPRO/devkitPPC"
[ -d "$DEVKITARM" ] || DEVKITARM="$DEVKITPRO/devkitARM"
export DEVKITPPC DEVKITARM

EXE=""; [ "$HOST" = windows ] && EXE=".exe"
missing=0
for t in "$DEVKITPPC/bin/powerpc-eabi-gcc$EXE" "$DEVKITARM/bin/arm-none-eabi-gcc$EXE"; do
	[ -x "$t" ] || { echo "Missing: $t" >&2; missing=1; }
done
if [ "$missing" = 1 ]; then
	echo "Run scripts/setup-toolchain.sh first." >&2
	exit 1
fi

# elf2dol/bin2s live here, and wii_rules does not add $DEVKITPPC/bin itself.
export PATH="$DEVKITPPC/bin:$DEVKITARM/bin:$DEVKITPRO/tools/bin:$PATH"

# --- Windows quirks --------------------------------------------------------
mkvars=(
	DEVKITPRO="$DEVKITPRO"
	DEVKITPPC="$DEVKITPPC"
	DEVKITARM="$DEVKITARM"
)

if [ "$HOST" = windows ]; then
	# The assembler and linker write temporaries through the Windows API, so
	# TMP/TEMP must be native paths with backslashes. A POSIX /tmp mapping
	# gives: Cannot create temporary file in C:\WINDOWS\: Permission denied
	mkdir -p "$REPO_ROOT/.build_tmp"
	BUILD_TMP="$(cd "$REPO_ROOT/.build_tmp" && pwd -W 2>/dev/null | sed 's|/|\\|g')"
	[ -n "$BUILD_TMP" ] && mkvars+=(TMP="$BUILD_TMP" TEMP="$BUILD_TMP")
fi

target="${1:-all}"
shift || true

case "$target" in
	all)    make "${mkvars[@]}" "$@" ;;
	kernel) make "${mkvars[@]}" kernel "$@" ;;
	loader) make "${mkvars[@]}" loader "$@" ;;
	clean)
		# Keep going if a subproject's clean fails: otherwise the first failure
		# aborts the rest and leaves stale objects that break the next build
		# with confusing undefined references.
		make "${mkvars[@]}" -k clean || true
		rm -rf loader/build loader/data/kernel.zip .build_tmp
		;;
	*) echo "usage: scripts/build.sh [all|kernel|loader|clean] [make args...]" >&2; exit 2 ;;
esac
