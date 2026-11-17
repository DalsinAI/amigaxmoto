#!/usr/bin/env python3
"""Host tests for the port's package and Installer (no stove, no Amiga):

  * pack.py makes the archive from a drawer like `make dist`'s: the drawer
    and its icon, and Install-DRAWER with its icon at the root; a packed-
    already file stored -lh0-; the archive reads back; package.json names the
    program's SHA-256;
  * an Installer made for another version, or a drawer without its icon, is
    refused;
  * build-amiga.sh builds soft float for the 68040 (fpuauto, never -m68881)
    with the port's own Makefile and checks for FPU code;
  * the Installer spec names this drawer, program and version, and the
    generated Install-DRAWER is this version's;
  * the port's Makefile lets CPU be set from outside (CPU ?=);
  * fpu_check.py passes constant tables and exception frame records, and
    fails FPU code in a function;
  * stove-dir.sh prefers the kitchen stove.

    python3 package/tests/test_package.py
MIT, Copyright (c) 2026 AmiCradle Team.
"""
from __future__ import annotations

import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PKG = ROOT / "package"
VERSION = (PKG / "VERSION").read_text().strip()
ENV = dict(re.findall(r"^([A-Z]+)=\"?([^\"\n]*)\"?$", (PKG / "port.env").read_text(), re.M))
DRAWER, PROG, ARCHIVE = ENV["DRAWER"], ENV["PROG"], ENV["ARCHIVE"]
sys.path.insert(0, str(ROOT / "tools"))
import lha_archive  # noqa: E402

fails = 0
checks = 0


def check(cond: bool, what: str) -> None:
    global fails, checks
    checks += 1
    if not cond:
        fails += 1
        print(f"FAIL {what}")


def pack(dist: Path, out: Path, installer: Path = PKG / "installer") -> subprocess.CompletedProcess:
    return subprocess.run([sys.executable, str(PKG / "pack.py"), "--dist", str(dist), "--drawer", DRAWER,
                           "--program", PROG, "--archive", ARCHIVE, "--version", VERSION,
                           "--installer", str(installer), "--source", "a" * 40, "--out", str(out)],
                          capture_output=True, text=True)


with tempfile.TemporaryDirectory() as tmp:
    tmp = Path(tmp)
    dist = tmp / "dist"
    (dist / DRAWER / "data").mkdir(parents=True)
    (dist / DRAWER / PROG).write_bytes(b"\x00\x00\x03\xf3 a hunk program")
    (dist / DRAWER / "data" / "title.png").write_bytes(b"\x89PNG" + bytes(64))
    (dist / DRAWER / "data" / "level.txt").write_bytes(b"level " * 40)
    (dist / f"{DRAWER}.info").write_bytes(b"\xe3\x10drawer")
    r = pack(dist, tmp / "out")
    check(r.returncode == 0, f"pack.py runs ({r.stderr.strip()[-300:]})")
    lha = tmp / "out" / f"{ARCHIVE}-{VERSION}-68040.lha"
    check(lha.is_file(), "the archive is named for the version and the 68040")
    if lha.is_file():
        raw = lha.read_bytes()
        back = {m.path: m.data for m in lha_archive.read(raw)}
        for p in (f"{DRAWER}/{PROG}", f"{DRAWER}/data/title.png", f"{DRAWER}.info",
                  f"Install-{DRAWER}", f"Install-{DRAWER}.info"):
            check(p in back, f"{p} is in the archive")
        check(back.get(f"Install-{DRAWER}") == (PKG / "installer" / f"Install-{DRAWER}").read_bytes(),
              "the Installer at the root is package/installer's")
        png = f"{DRAWER}/data/title.png".encode()
        check(raw[raw.index(png) - 20:raw.index(png) - 15] == b"-lh0-", "a .png is stored -lh0-")
        txt = f"{DRAWER}/data/level.txt".encode()
        check(raw[raw.index(txt) - 20:raw.index(txt) - 15] == b"-lh5-", "a text file is packed -lh5-")
        summary = json.loads((tmp / "out" / "package.json").read_text())
        check(summary["sha256"] in (tmp / "out" / f"{lha.name}.sha256").read_text(), "the .sha256 names the archive")
        import hashlib
        check(summary["executable_sha256"] == hashlib.sha256(b"\x00\x00\x03\xf3 a hunk program").hexdigest(),
              "package.json names the program's SHA-256")
        check((tmp / "out" / f"{DRAWER}.tar").is_file(), "the drawer's tar for a lab")
    other = tmp / "inst"
    other.mkdir()
    (other / f"Install-{DRAWER}").write_bytes(f"; $VER: Install-{DRAWER} 0.0-old (1.1.2026)\n".encode())
    (other / f"Install-{DRAWER}.info").write_bytes(b"\xe3\x10")
    r = pack(dist, tmp / "out2", other)
    check(r.returncode != 0 and "another version" in r.stdout + r.stderr, "an Installer for another version is refused")
    (dist / f"{DRAWER}.info").unlink()
    r = pack(dist, tmp / "out3")
    check(r.returncode != 0 and "icon" in r.stdout + r.stderr, "a drawer without its icon is refused")

script = (PKG / "build-amiga.sh").read_text()
check('CPU="-m68040 -msoft-float"' in script and 'CPU="$CPU"' in script, "the port's Makefile is run soft float for the 68040")
check("! grep -q -- '-m68881'" in script, "build-amiga.sh fails a build that has -m68881")
check("fpu_check.py" in script and "___fpuauto_select" in script, "the build checks for FPU code and fpuauto")
check("amiga/Makefile.amigaos" in script and " dist" in script, "the drawer is the port's own make dist")
mk = (ROOT / "amiga" / "Makefile.amigaos").read_text(encoding="latin-1")
check(re.search(r"^CPU\s*\?=", mk, re.M) is not None, "the Makefile's CPU can be set from outside")
check(re.search(rf"^OUT\s*=.*\b{re.escape(DRAWER)}\s*$", mk, re.M) is not None, "make dist makes the drawer DRAWER")

spec = json.loads((PKG / "installer.json").read_text())
check(spec["version"] == VERSION, "installer.json's version is package/VERSION")
check(spec["drawer"] == DRAWER and spec["program"] == PROG, "the Installer starts DRAWER/PROG")
check(spec.get("cpu") == "68040" and spec.get("fpu") is True, "the Installer asks for a 68040 and an FPU (GFOpenGPU's modules)")
check(all(m.endswith(".module") for m in spec.get("modules", [])) and "SDL2.module" in spec.get("modules", []),
      "the Installer checks for GFOpenGPU's modules")
inst = (PKG / "installer" / f"Install-{DRAWER}").read_text(encoding="latin-1")
check(inst.startswith(f"; $VER: Install-{DRAWER} {VERSION} ("), "the generated Installer is this version's")
for m in spec.get("modules", []):
    check(m in inst, f"the Installer checks for {m}")
for k in spec.get("keep", []):
    check(k in inst, f"an update keeps {k}")
for d in spec.get("data", []):
    check(d["into"] in inst or not d["into"], f"the Installer puts {d['what']} into {d['into'] or 'the drawer'}")

sys.path.insert(0, str(PKG))
import fpu_check  # noqa: E402
check(fpu_check.is_data("__EH_FRAME_BEGIN__", "__EH_FRAME_BEGIN__"), "fpu_check reads the exception frame records as data")
check(not fpu_check.is_data("_main", "_main"), "fpu_check fails an FPU instruction in a C function")
check(fpu_check.is_data("__ZN4Math13sineWindow512E", "Math::sineWindow512"), "fpu_check reads a C++ table as data")
check(not fpu_check.is_data("__ZN4Menu4DrawEP12SDL_Renderer", "Menu::Draw(SDL_Renderer*)"),
      "fpu_check fails an FPU instruction in a C++ function")

allow = PKG / "fpu-allow.txt"
if allow.exists():
    for line in allow.read_text().splitlines():
        if line.strip() and not line.startswith("#"):
            words, _, why = line.partition("#")
            check(len(words.split()) == 2 and why.strip() != "", f"fpu-allow.txt: OBJECT SYMBOL  # why ({line})")
    check(fpu_check.read_allow(allow.read_text()) != set(), "fpu_check reads fpu-allow.txt")

with tempfile.TemporaryDirectory() as home:
    r = subprocess.run(["sh", str(PKG / "stove-dir.sh")], capture_output=True, text=True, env={**os.environ, "HOME": home})
    check(r.stdout.strip() == f"{home}/AmiCradle/kitchen/stoves/os32-gcc16", "no stove: the kitchen path")

print(f"package: {checks} checks, {fails} failed")
sys.exit(1 if fails else 0)
