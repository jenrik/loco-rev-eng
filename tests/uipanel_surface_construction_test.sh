#!/usr/bin/env bash
# Guards UIPANEL_Surface's rule-of-three (0x42A110 ctor / 0x42A140 dtor /
# 0x42A1C0 copy ctor). This tree's native link uses
# `-Wl,--unresolved-symbols=ignore-all` (meson.build), so a caller left
# referencing one of the old free-function names below would link
# "successfully" and only crash at runtime the first time it's reached --
# this test catches that case at build-verification time instead.
set -euo pipefail

binary=${1:?usage: $0 path/to/lego_loco}

# Itanium ABI emits both a complete-object (C1/D1) and base-object (C2/D2)
# symbol; both demangle to the same name and, when identical, alias to the
# same address -- count distinct ADDRESSES, not symbol lines. The
# destructor is virtual, so it additionally gets a genuinely distinct
# deleting-destructor (D0) address; ctor/copy-ctor have no such variant.
check_one() {
    local sym=$1 max=$2
    addr_count=$(nm -anC "$binary" | grep -F " T $sym" | awk '{print $1}' | sort -u | wc -l)
    if [[ "$addr_count" -lt 1 || "$addr_count" -gt "$max" ]]; then
        echo "FAIL: expected 1-$max concrete '$sym' (by address), found $addr_count" >&2
        exit 1
    fi
}
check_one 'UIPANEL_Surface::UIPANEL_Surface()' 1
check_one 'UIPANEL_Surface::~UIPANEL_Surface()' 2
check_one 'UIPANEL_Surface::UIPANEL_Surface(UIPANEL_Surface const&)' 1

# The pre-2026-08-14 free-function facades must not exist as undefined
# references anywhere in the link -- every caller now goes through
# `new`/`delete`/the copy constructor directly.
if nm -C -u "$binary" | grep -Eq 'UIPANEL_CreateSurface\(|UIPANEL_DestroySurface\(|UIPANEL_CopySurface\('; then
    echo "FAIL: a stale UIPANEL_CreateSurface/DestroySurface/CopySurface free-function reference is still linked" >&2
    exit 1
fi

echo "PASS: UIPANEL_Surface ctor/dtor/copy-ctor are the sole linked constructors, no stale free-function references remain"
