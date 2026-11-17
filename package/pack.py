#!/usr/bin/env python3
"""The port's LhA archive, from the drawer the port's own `make dist` made:

    python3 package/pack.py --dist DIST --drawer DRAWER --program PROG --archive NAME
        --version VERSION --installer DIR [--source SHA] [--compiler TEXT] --out OUT

DIST holds DRAWER/ (with PROG in it) and DRAWER.info. The archive,
OUT/NAME-VERSION-68040.lha, holds the drawer and its icon, and at its root
Install-DRAWER and Install-DRAWER.info from DIR (the port's Installer, made by
amicradle's scripts/ports/port_installer.py, which must name this VERSION in
its $VER), as every port's archive must (Dale, 16 Nov 2026 17:07). Files
that are packed already (.png, .ogg and the like) are stored -lh0-, the rest
packed -lh5- where that helps. The archive is read back member by member.
Also written: the .sha256, DRAWER.tar (the archive's members, for a lab) and
package.json (the archive's name, size and SHA-256, the program's SHA-256,
the source commit, the compiler and every member).
MIT, Copyright (c) 2026 AmiCradle Team.
"""
from __future__ import annotations

import argparse
import hashlib
import io
import json
import os
import sys
import tarfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import lha_archive  # noqa: E402

STORE = (".png", ".jpg", ".jpeg", ".ogg", ".mp3", ".flac", ".gz", ".zip", ".bz2", ".xz", ".7z", ".lha", ".webp",
         ".ttf", ".otf", ".grp", ".pk3", ".dat.gz")


def sha256(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()


def members(dist: Path, drawer: str, installer: Path, version: str, stamp: float) -> list:
    root = dist / drawer
    out = []
    for p in sorted(root.rglob("*")):
        if p.is_file():
            rel = p.relative_to(dist).as_posix()
            prot = 0
            out.append(lha_archive.Member(rel, p.read_bytes(), prot, "", stamp))
    icon = dist / f"{drawer}.info"
    if not icon.is_file():
        raise SystemExit(f"{icon} is missing: the drawer needs its icon")
    out.append(lha_archive.Member(f"{drawer}.info", icon.read_bytes(), 0, "", stamp))
    script = installer / f"Install-{drawer}"
    head = script.read_bytes().split(b"\n", 1)[0].decode("latin-1")
    if not head.startswith(f"; $VER: Install-{drawer} {version} ("):
        raise SystemExit(f"{script.name} was made for another version ({head!r}): run port_installer.py again")
    for name in (f"Install-{drawer}", f"Install-{drawer}.info"):
        out.append(lha_archive.Member(name, (installer / name).read_bytes(), 0, "", stamp))
    names = [m.path.lower() for m in out]
    if len(set(names)) != len(names):
        raise SystemExit("two members share a name (AmigaDOS names ignore case)")
    return out


def pack(ms: list) -> bytes:
    return b"".join(lha_archive.pack([m], "lh0" if m.path.lower().endswith(STORE) else "auto")[:-1]
                    for m in ms) + b"\0"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    for a in ("--dist", "--drawer", "--program", "--archive", "--version", "--installer", "--out"):
        ap.add_argument(a, required=True)
    ap.add_argument("--source", default="unknown")
    ap.add_argument("--compiler", default="")
    a = ap.parse_args()
    dist, out, installer = Path(a.dist), Path(a.out), Path(a.installer)
    stamp = float(os.environ.get("SOURCE_DATE_EPOCH") or 0) or None
    os.environ["TZ"] = "UTC"
    prog = dist / a.drawer / a.program
    if not prog.is_file():
        raise SystemExit(f"{prog} is missing")
    ms = members(dist, a.drawer, installer, a.version, stamp)
    out.mkdir(parents=True, exist_ok=True)
    base = f"{a.archive}-{a.version}-68040"
    lha = out / f"{base}.lha"
    lha.write_bytes(pack(ms))
    back = lha_archive.read(lha.read_bytes())
    if [(m.path, m.data) for m in back] != [(m.path, m.data) for m in ms]:
        raise SystemExit("the archive does not read back as written")
    digest = sha256(lha.read_bytes())
    (out / f"{base}.lha.sha256").write_text(f"{digest}  {lha.name}\n")
    with tarfile.open(out / f"{a.drawer}.tar", "w", format=tarfile.USTAR_FORMAT) as t:
        for m in ms:
            info = tarfile.TarInfo(m.path)
            info.size, info.mode = len(m.data), 0o644
            t.addfile(info, io.BytesIO(m.data))
    summary = {"archive": lha.name, "bytes": lha.stat().st_size, "sha256": digest, "version": a.version,
               "executable_sha256": sha256(prog.read_bytes()), "source_commit": a.source,
               "compiler_release": a.compiler,
               "members": [{"path": m.path, "bytes": len(m.data), "sha256": sha256(m.data)} for m in ms]}
    (out / "package.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps({k: v for k, v in summary.items() if k != "members"}, indent=2))
    print(f"{len(ms)} members")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
