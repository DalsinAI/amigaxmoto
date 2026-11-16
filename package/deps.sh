#!/bin/sh
# deps.sh PREFIX WORK NAME... - the static libraries and data the stove
# doesn't carry, for package/build-amiga.sh (port.env's DEPLIBS).
# PREFIX gets the stove's port-deps (zlib, libpng, jpeg, FreeType) and then
# each NAME, built soft float for the 68040 by the port's own
# amiga/deps/build-m68k-libs.sh. Each source is fetched at its pinned
# SHA-256 into WORK (nothing fetched is kept in the repository; zlib
# replaces the stove's); bzip2 and
# FreeType are the tarballs already kept in amiga/deps. pak64 is Simutrans'
# graphics set (the zip, at WORK/simupak64-125-0.zip).
# STOVE is the stove's prefix (bin/m68k-amigaos-gcc). MIT, Copyright (c)
# 2026 AmiCradle Team.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
PREFIX=$1 WORK=$2
shift 2
fetch() { # name url sha256
	[ -f "$WORK/$1" ] || curl -fsSL --retry 3 -o "$WORK/$1" "$2"
	echo "$3  $WORK/$1" | sha256sum -c - > /dev/null || { echo "$1: not the pinned SHA-256 $3"; exit 1; }
}

mkdir -p "$PREFIX" "$WORK"

# DEPCACHE (build-amiga.sh sets it with CCACHE=1): the finished PREFIX is
# kept as a tar named for the SHA-256 of everything that makes it (these
# scripts, the port's recipes, the names asked for and the compiler), so a
# second stove build needn't rebuild libxml2 and SQLite on a busy daletop.
KEY=""
if [ -n "${DEPCACHE:-}" ]; then
	KEY=$( { cat "$HERE/deps.sh" "$ROOT/amiga/deps/build-m68k-libs.sh"; find "$ROOT/amiga/deps" -type f -name '*.c' | sort | xargs -r cat;
		echo "$@"; "$STOVE/bin/m68k-amigaos-gcc" --version; } | sha256sum | cut -c1-64)
	if [ -f "$DEPCACHE/$KEY.tar" ]; then
		tar -xf "$DEPCACHE/$KEY.tar" -C "$PREFIX"
		echo "deps.sh: $* from $DEPCACHE/$KEY.tar"
		for lib in "$@"; do [ "$lib" != pak64 ] || fetch_pak64=1; done
		[ -z "${fetch_pak64:-}" ] || set -- pak64
		[ -n "${fetch_pak64:-}" ] || exit 0
		KEY=""
	fi
fi
[ -f "$PREFIX/.stove-copied" ] || cp -R "$STOVE/m68k-amigaos/port-deps/include" "$STOVE/m68k-amigaos/port-deps/lib" "$PREFIX/"
# Everything newer than this is built here, for build-amiga.sh's FPU check
# (the stove's own copies are its business).
[ -f "$PREFIX/.stove-copied" ] || { sleep 1; touch "$PREFIX/.stove-copied"; }

for lib in "$@"; do
	case $lib in
	zlib)
		# The stove's zconf.h defines a lower-case far, which C++ code using
		# far as a name (LinCity-NG) can't compile with; zlib's own doesn't.
		fetch zlib-1.3.1.tar.gz https://zlib.net/fossils/zlib-1.3.1.tar.gz \
			9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23
		tar -xzf "$WORK/zlib-1.3.1.tar.gz" -C "$WORK" ;;
	xml2)
		fetch libxml2-2.12.9.tar.xz https://download.gnome.org/sources/libxml2/2.12/libxml2-2.12.9.tar.xz \
			59912db536ab56a3996489ea0299768c7bcffe57169f0235e7f962a91f483590
		tar -xJf "$WORK/libxml2-2.12.9.tar.xz" -C "$WORK" ;;
	sqlite)
		fetch sqlite-amalgamation-3460100.zip https://www.sqlite.org/2024/sqlite-amalgamation-3460100.zip \
			77823cb110929c2bcb0f5d48e4833b5c59a8a6e40cdea3936b99e199dbbe5784
		( cd "$WORK" && unzip -qo sqlite-amalgamation-3460100.zip )
		cp "$ROOT"/amiga/deps/sqlite/*.c "$WORK/sqlite-amalgamation-3460100/" ;;
	pak64)
		fetch simupak64-125-0.zip https://downloads.sourceforge.net/project/simutrans/pak64/125-0/simupak64-125-0.zip \
			850108b76505ca306822b88c22a1829c7102a6c9d7390c057f533ba70d9ecdad
		continue ;;
	bzip2|freetype) ;;
	*) echo "deps.sh: no recipe for $lib"; exit 2 ;;
	esac
	STOVE="$STOVE" DEPS="$WORK" PREFIX="$PREFIX" sh "$ROOT/amiga/deps/build-m68k-libs.sh" "$lib"
done
if [ -n "$KEY" ]; then
	mkdir -p "$DEPCACHE" && tar -cf "$DEPCACHE/$KEY.tar.part" -C "$PREFIX" . && mv "$DEPCACHE/$KEY.tar.part" "$DEPCACHE/$KEY.tar"
fi
