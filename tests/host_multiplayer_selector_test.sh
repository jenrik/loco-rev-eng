#!/usr/bin/env bash
# Guards the recovered menu branches that show the original right-hand
# multiplayer control (0x409) in the known-good state-7/provider-list path.
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

require 'kHostPlay,'
require 'kHostScenario,'
require 'if (!multiplayer && host_point_in_rect(menu.btnPlayRect, x, y)) return kHostPlay;'
require 'if (multiplayer && has_scenario && host_point_in_rect(menu.btnScenarioRect, x, y)) return kHostScenario;'
require 'if (has_scenario) host_blit_menu_sprite(this->sprite_409, this->btnScenarioRect);'
require 'case kHostPlay:'
require '_g_netman_state[7] = 1;'
require 'NETMAN_SetGameMode(g_netman, 3);'
require 'case kHostScenario:'
require '_g_netman_state[7] = 0;'
require 'NETMAN_SetGameMode(g_netman, 0);'
require 'SDL3_GameAudioPlayResource(0x5015);'
require 'this->setState(_g_netman_state[7] != 0 ? 5 : 4);'

echo 'PASS: host main menu preserves the recovered multiplayer push-state toggle and click sound'
