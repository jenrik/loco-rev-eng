#!/usr/bin/env bash
# Ensures the SDL resource-0x403 accept control follows the same guarded name
# commit path as Enter. Assembly evidence: its pressed branch calls
# EditWindow_OnPlayerNameChanged at 0x422AB2.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
source="$root/src/decompiled_cpp/ui/EditWindow.cpp"
header="$root/src/decompiled_cpp/ui/EditWindow.h"

require() {
    local pattern="$1"
    local file="$2"
    grep -Fq "$pattern" "$file" || {
        echo "FAIL: missing '$pattern' in ${file#$root/}" >&2
        exit 1
    }
}

require 'if (button == kHostOptionOne) {' "$source"
require 'this->hostCommitPlayerName();' "$source"
require 'void EditWindow::hostCommitPlayerName()' "$source"
require 'this->setState(3);' "$source"
require 'void hostCommitPlayerName();' "$header"

# Both controls must converge on this helper. It appears once in the accept
# branch and once in the Enter branch.
count=$(grep -Fc 'this->hostCommitPlayerName();' "$source")
[[ "$count" -eq 2 ]] || {
    echo "FAIL: expected accept and Enter to share two host commit calls, got $count" >&2
    exit 1
}

line=$(grep -n 'void EditWindow::hostCommitPlayerName()' "$source" | cut -d: -f1)
start=$(head -n "$line" "$source" | grep -n '#ifndef _WIN32' | tail -n1 | cut -d: -f1)
end=$(tail -n "+$line" "$source" | grep -n '#endif  // !_WIN32' | head -n1 | cut -d: -f1)
[[ -n "$start" && -n "$end" ]] || {
    echo 'FAIL: host accept path is not inside #ifndef _WIN32' >&2
    exit 1
}

echo 'PASS: main-menu accept control uses the guarded Enter-equivalent commit path'
