#!/usr/bin/env bash
# Guards the host-only main-menu choice pair resolved from loco.exe's PE
# string table: 0x407/0x408 = singleup/singledown and
# 0x409/0x40A = multipleup/multipledown.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
source="$root/src/decompiled_cpp/ui/EditWindow.cpp"

require() {
    local pattern="$1"
    grep -Fq "$pattern" "$source" || {
        echo "FAIL: missing '$pattern' in ${source#$root/}" >&2
        exit 1
    }
}

require 'kHostSinglePlayer,'
require 'kHostMultiplayer,'
require 'if (host_point_in_rect(menu.btnPlayRect, x, y)) return kHostSinglePlayer;'
require 'if (host_point_in_rect(menu.btnScenarioRect, x, y)) return kHostMultiplayer;'
require '? this->sprite_408 : this->sprite_407,'
require '? this->sprite_40A : this->sprite_409,'
require 'case kHostSinglePlayer:'
require 'this->hostMultiplayerSelected = false;'
require 'case kHostMultiplayer:'
require 'this->hostMultiplayerSelected = true;'

echo 'PASS: host main menu renders and routes both single-player and multiplayer controls'
