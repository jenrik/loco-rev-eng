#!/usr/bin/env bash
# Apply two binary patches to loco.exe that bypass the 16-bit colour-depth check.
#
# Patch 1 — fcn.00446050 @ VA 0x446063, file offset 0x45463
#   jne +11 (75 0B) → jmp +11 (EB 0B)
#   DirectDraw init return-value check: makes fcn.00446050 always continue
#   even when fcn.0045b500 fails under Wine/Xvfb.
#
# Patch 2 — fcn.00406680 @ VA 0x4066CA, file offset 0x5ACB (displacement byte)
#   jmp 0x406752 (E9 83 …) → jmp 0x4066D0 (E9 01 …)
#   GDI colour check: the shipped binary has an unconditional jmp to the
#   "wrong colour depth" error path.  Changing the displacement redirects
#   to the mouse-presence check (0x4066D0), effectively skipping the error.

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

apply_patch() {
    local label="$1" offset="$2" expected="$3" replacement="$4"
    local current
    current=$(xxd -s "$offset" -l "${#expected}" -p "$EXE" | tr -d '\n')
    # Convert hex string byte count: 2 hex chars = 1 byte
    local nbytes=$(( ${#expected} / 2 ))
    current=$(xxd -s "$offset" -l "$nbytes" -p "$EXE" | tr -d '\n')
    if [ "$current" = "$expected" ]; then
        printf "%b" "$(echo "$replacement" | sed 's/../\\x&/g')" \
            | dd of="$EXE" bs=1 seek="$offset" conv=notrunc 2>/dev/null
        echo "Patch $label applied."
    elif [ "$current" = "$replacement" ]; then
        echo "Patch $label already applied."
    else
        echo "Error [$label]: unexpected bytes at offset $offset: $current"
        echo "  expected original : $expected"
        echo "  expected patched  : $replacement"
        exit 1
    fi
}

apply_patch "1" $((0x45463)) "750b" "eb0b"
apply_patch "2" $((0x5acb))  "83"   "01"

echo "Done — loco.exe is patched."
