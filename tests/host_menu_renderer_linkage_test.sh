#!/usr/bin/env bash
# Verifies that GameSetupPanel's SDL host renderer links its C-exported renderer
# lookup directly. A mangled unresolved declaration is silently tolerated by the
# permissive host linker and previously jumped to __stack_chk_fail after Enter.
set -euo pipefail

binary="${1:?usage: $0 /path/to/lego_loco}"
read -r start size < <(nm -C -S --defined-only "$binary" | awk '/ T GameSetupPanel::hostRenderFrame\(\)$/ { print $1, $2; exit }')
[[ -n "${start:-}" && -n "${size:-}" ]] || { echo "FAIL: host renderer symbol missing" >&2; exit 1; }
end=$(printf '0x%x' "$((16#$start + 16#$size))")
disassembly=$(objdump -d --demangle --start-address="0x$start" --stop-address="$end" "$binary")
printf '%s\n' "$disassembly" | grep -qE 'call.*<SDL3_GetRenderer>' || {
    echo "FAIL: host renderer does not call SDL3_GetRenderer directly" >&2; exit 1;
}
if printf '%s\n' "$disassembly" | grep -qE 'call.*<__stack_chk_fail'; then
    echo "FAIL: host renderer still calls __stack_chk_fail" >&2; exit 1
fi
echo "PASS: GameSetupPanel host renderer resolves SDL3_GetRenderer directly"
