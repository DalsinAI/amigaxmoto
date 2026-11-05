#!/usr/bin/env python3
"""Writes xmoto.bin from bin/package.lst, as `xmoto --pack` does
(src/common/Packager.cpp), without a host build of X-Moto: "XBI3", the md5
of package.lst, then for each file its name, its md5, its size (32-bit
little-endian) and its bytes. Lengths are one byte each.

    python3 amiga/tools/pack_xmoto_bin.py bin out/xmoto.bin

The AmiCradle Team. GPL 2 or later, like X-Moto.
"""
import hashlib
import os
import struct
import sys


def md5file(path):
    h = hashlib.md5()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest().encode("ascii")


def short(b, what):
    if len(b) > 255:
        sys.exit("pack_xmoto_bin: %s longer than 255 bytes: %r" % (what, b))
    return bytes([len(b)]) + b


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    datadir, out = sys.argv[1], sys.argv[2]
    pkglist = os.path.join(datadir, "package.lst")
    names = []
    with open(pkglist, "rb") as f:
        for line in f:
            s = line.strip(b" \t\r\n")
            if s:
                names.append(s)
    missing = 0
    with open(out, "wb") as o:
        o.write(b"XBI3")
        o.write(short(md5file(pkglist), "md5"))
        for name in names:
            path = os.path.join(datadir, name.decode("utf-8"))
            if not os.path.isfile(path):
                print("!!! FAILED TO ADD %r" % name.decode("utf-8"))
                missing += 1
                continue
            with open(path, "rb") as f:
                data = f.read()
            o.write(short(name, "name"))
            o.write(short(md5file(path), "md5"))
            o.write(struct.pack("<i", len(data)))
            o.write(data)
    print("%s: %d files%s" % (out, len(names) - missing,
                              ", %d missing" % missing if missing else ""))


if __name__ == "__main__":
    main()
