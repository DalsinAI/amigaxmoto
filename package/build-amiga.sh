#!/bin/sh
# The release archive, for Cradle's stove_build (target package): the port's
# own amiga/Makefile.amigaos builds the program and its drawer (make dist),
# for the 68040 in soft float (CPU="-m68040 -msoft-float", so the stove's
# fpuauto takes the FPU when there is one), then package/pack.py makes
# ARCHIVE-VERSION-68040.lha: the drawer and its icon, with the port's
# Installer (Install-DRAWER and its icon, made by amicradle's
# scripts/ports/port_installer.py from package/installer.json) beside it at
# the archive's root, as every port's must be (Dale, 16 Nov 2026 17:07). It
# also writes the .sha256, the drawer as DRAWER.tar (for a lab) and
# package.json.
# package/fpu_check.py stops the build on any FPU instruction in the port's
# objects; fpuauto must be linked in.
# STOVE is the os32-gcc16 stove (or its prefix/); package/port.env names the
# drawer, the archive and the program (and DEPLIBS, MAKEVARS); VERSION is
# package/VERSION.
# Nothing is published from here.
# MIT, Copyright (c) 2026 AmiCradle Team.
set -eu

JOBS=${JOBS:-2}
CPU="-m68040 -msoft-float"

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
. "$HERE/port.env"                       # DRAWER, PROG, ARCHIVE
OUT=${OUT:-"$ROOT/build-amiga-package"}
VERSION=$(cat "$HERE/VERSION")
STOVE=${STOVE:-"$(sh "$HERE/stove-dir.sh")"}
if [ ! -x "$STOVE/bin/m68k-amigaos-gcc" ] && [ -x "$STOVE/prefix/bin/m68k-amigaos-gcc" ]; then
	STOVE="$STOVE/prefix"
fi
[ -x "$STOVE/bin/m68k-amigaos-gcc" ] || { echo "no m68k-amigaos-gcc in $STOVE (set STOVE)"; exit 2; }
mkdir -p "$OUT"
OUT=$(CDPATH= cd -- "$OUT" && pwd)
BUILD=$(mktemp -d "${TMPDIR:-/tmp}/$ARCHIVE-package.XXXXXX")
trap 'rm -rf "$BUILD"' EXIT INT TERM

# A copy of the checkout, so the build's own build/ and dist/ stay out of it.
SRC="$BUILD/src"
mkdir -p "$SRC"
( cd "$ROOT" && tar --exclude=.git --exclude=build-amiga-package --exclude=build/amigaos --exclude=./dist -cf - . ) |
	( cd "$SRC" && tar -xf - )
# The SDK is the stove's own (SDL 2, GL, port-deps); the Makefile's old
# default paths are gone. GPUSDK is the GFOpenGPU developer kit, for what the
# stove doesn't carry (PhysicsFS, OpenAL); port.env's MAKEVARS adds the port's
# own make settings, which may name $SDK and $GPUSDK.
SDK="$STOVE/m68k-amigaos"
GPUSDK=${GPUSDK:-/home/da1ek/AmiCradle-dev/scratch/rc-20261115/gpu-sdk/GFOpenGPU-SDK-0.7}
# port.env's DEPLIBS: the static libraries (and data) the stove doesn't
# carry, built by package/deps.sh soft float for the 68040 into the build's
# own DEPS, beside a copy of the stove's port-deps.
DEPS="$SDK/port-deps"
DEPWORK="$BUILD/deps-src"
if [ -n "${DEPLIBS:-}" ]; then
	DEPS="$BUILD/deps"
	# shellcheck disable=SC2086
	STOVE="$STOVE" sh "$HERE/deps.sh" "$DEPS" "$DEPWORK" $DEPLIBS > "$OUT/deps-output.txt" 2>&1 ||
		{ tail -n 40 "$OUT/deps-output.txt"; exit 1; }
fi
set -- STOVE="$STOVE" CPU="$CPU" SDL2KIT="$SDK" KIT="$SDK" DEPS="$DEPS" NDK="$SDK/include"
[ -z "${MAKEVARS:-}" ] || eval "set -- \"\$@\" $MAKEVARS"
# port.env's CCACHE=1: a port too big for the stove's 900 s compiles through
# ccache (CCACHE_DIR, by default Team 2's on daletop), so a second build
# finishes what the first one started. The objects are the compiler's own.
if [ "${CCACHE:-}" = 1 ] && command -v ccache > /dev/null; then
	export CCACHE_DIR=${CCACHE_DIR:-/home/da1ek/AmiCradle-dev/teams/team2/scratch/ccache-ports}
	export CCACHE_BASEDIR="$SRC" CCACHE_NOHASHDIR=1
	mkdir -p "$CCACHE_DIR"
	set -- "$@" CC="ccache $STOVE/bin/m68k-amigaos-gcc" CXX="ccache $STOVE/bin/m68k-amigaos-g++"
fi
( cd "$SRC" && make -f amiga/Makefile.amigaos -j"$JOBS" "$@" all && \
	make -f amiga/Makefile.amigaos "$@" dist ) \
	> "$OUT/build-output.txt" 2>&1 || { tail -n 60 "$OUT/build-output.txt"; exit 1; }

# Every compile and the link soft float for the 68040, never -m68881, nothing
# compiled for the FPU, fpuauto linked in.
grep -E 'm68k-amigaos-g(cc|\+\+) ' "$OUT/build-output.txt" | grep -qE ' -c | -o ' ||
	{ echo "the build shows no compile lines to check (make it print them)"; exit 1; }
if grep -E 'm68k-amigaos-g(cc|\+\+) ' "$OUT/build-output.txt" | grep -E ' -c | -o ' | grep -v -- '-m68040 -msoft-float' | grep -q .; then
	echo "a compile or link without -m68040 -msoft-float"; exit 1
fi
! grep -q -- '-m68881' "$OUT/build-output.txt" || { echo "-m68881 is in the build"; exit 1; }
{ find "$SRC/build" -name '*.o'; [ ! -d "$BUILD/deps/lib" ] || find "$BUILD/deps/lib" -name '*.a' -newer "$BUILD/deps/.stove-copied"; } |
	sort > "$BUILD/objects.txt"
[ -s "$BUILD/objects.txt" ] || { echo "no objects under build/"; exit 1; }
# package/fpu-allow.txt, when there is one, names C constant tables (which
# fpu_check can't tell from functions), one "OBJECT SYMBOL  # why" a line.
set --
[ ! -f "$HERE/fpu-allow.txt" ] || set -- --allow "$HERE/fpu-allow.txt"
# shellcheck disable=SC2046
python3 "$HERE/fpu_check.py" "$STOVE/bin/m68k-amigaos-objdump" "$STOVE/bin/m68k-amigaos-c++filt" "$@" \
	$(cat "$BUILD/objects.txt") > "$OUT/fpu-check.txt" || { grep '^FAIL' "$OUT/fpu-check.txt"; exit 1; }
# The drawer's program is stripped: its symbols are in the build's PROG.debug.
SYMS="$SRC/build/amigaos/$PROG.debug"
[ -f "$SYMS" ] || SYMS="$SRC/dist/$DRAWER/$PROG"
"$STOVE/bin/m68k-amigaos-nm" "$SYMS" | grep -q '___fpuauto_select' ||
	{ echo "fpuauto is not linked in"; exit 1; }

python3 "$HERE/pack.py" --dist "$SRC/dist" --drawer "$DRAWER" --program "$PROG" --archive "$ARCHIVE" \
	--version "$VERSION" --installer "$HERE/installer" --source "$(git -C "$ROOT" rev-parse HEAD 2>/dev/null || echo unknown)" \
	--compiler "$("$STOVE/bin/m68k-amigaos-gcc" --version | head -n 1)" --out "$OUT"
