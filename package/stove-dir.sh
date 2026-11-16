#!/bin/sh
# Prints the default os32-gcc16 stove: ~/AmiCradle/kitchen/stoves/os32-gcc16 (the daletop layout since
# 15 Nov), else the old ~/AmiCradle/stoves/os32-gcc16 when only that one is there. build-amiga.sh uses
# this only when STOVE isn't set (stove_build sets it).
# MIT, Copyright (c) 2026 AmiCradle Team.
K="$HOME/AmiCradle/kitchen/stoves/os32-gcc16"
O="$HOME/AmiCradle/stoves/os32-gcc16"
if [ ! -d "$K" ] && [ -d "$O" ]; then echo "$O"; else echo "$K"; fi
