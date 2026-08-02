#!/usr/bin/env bash
# Guards the exact C++ ABI needed by TileMap::CreateOverlay (0x457080).
set -euo pipefail

binary=${1:?usage: $0 path/to/lego_loco}
canonical='UIPANEL_InitSurface(void*, int, int, int, unsigned int, unsigned char)'

if [[ $(nm -anC "$binary" | grep -F " T $canonical" | wc -l) -ne 1 ]]; then
    echo "FAIL: expected exactly one concrete canonical UIPANEL_InitSurface definition" >&2
    exit 1
fi

if nm -anC "$binary" | grep -Eq ' [Tt] UIPANEL_InitSurface$| [Tt] UIPANEL_InitSurface\(void\*, int, int, int, int,'; then
    echo "FAIL: shadow UIPANEL_InitSurface ABI variant linked" >&2
    exit 1
fi

function_assembly=$(objdump -d -C "$binary" | awk '
    index($0, "<TileMap::CreateOverlay(void*, unsigned char)>:") { in_function = 1; next }
    in_function && /^$/ { in_function = 0 }
    in_function { print }
')
grep -F "<${canonical}>" <<<"$function_assembly" >/dev/null

echo "PASS: TileMap::CreateOverlay calls the concrete canonical UIPANEL_InitSurface"
