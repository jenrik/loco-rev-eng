#!/usr/bin/env bash
# Patch: Skip 16-bit color depth check in loco.exe
#
# The game calls GetDeviceCaps(BITSPIXEL) and requires the result to be <= 16.
# At address 0x4066CA there is a conditional jump (jg) that skips past the
# error dialog when bits-per-pixel > 16. We change it to an unconditional jump.
#
# Before: 0F 8F 82 00 00 00  (jg  0x406752)
# After:  E9 83 00 00 00 90  (jmp 0x406752; nop)
#
# File offset 0x5ACA corresponds to virtual address 0x4066CA
# (.text section: VMA 0x401000, file offset 0x400)

set -euo pipefail

if [ $# -ne 1 ]; then
    echo "Usage: $0 <path-to-loco.exe>"
    exit 1
fi

EXE="$1"

if [ ! -f "$EXE" ]; then
    echo "Error: File not found: $EXE"
    exit 1
fi

# Verify the original bytes are what we expect
ORIGINAL=$(xxd -s 0x5aca -l 6 -p "$EXE")
if [ "$ORIGINAL" = "0f8f82000000" ]; then
    printf '\xe9\x83\x00\x00\x00\x90' | dd of="$EXE" bs=1 seek=$((0x5aca)) conv=notrunc 2>/dev/null
    echo "Patched: color depth check bypassed."
elif [ "$ORIGINAL" = "e98300000090" ]; then
    echo "Already patched."
else
    echo "Error: Unexpected bytes at offset 0x5ACA: $ORIGINAL"
    echo "Expected: 0f8f82000000 (original) or e98300000090 (patched)"
    exit 1
fi
