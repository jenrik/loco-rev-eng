#!/usr/bin/env bash
# Regression for lego_loco-4.core: main.cpp calls this host-only function
# immediately after GameLoop_Setup. It must be a linked definition, never an
# unresolved symbol silently accepted by the executable link.
set -euo pipefail

binary=${1:?usage: host_intro_video_linkage_test.sh <lego_loco>}
if ! nm -C --defined-only "$binary" | awk '
    index($0, "loco::intro::startLaunchSequence()") { found = 1 }
    END { exit !found }
'; then
    echo "FAIL: startLaunchSequence() is not defined in $binary" >&2
    exit 1
fi

echo "PASS: intro player is linked into the executable"
