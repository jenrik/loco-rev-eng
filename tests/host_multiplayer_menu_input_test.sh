#!/usr/bin/env bash
# Verifies that the SDL host routes mode-2 lobby clicks to the recovered
# GAMESTATE_HandleClick (0x40A4E0) control adapter rather than stale EditWindow
# menu hit rectangles. The adapter is deliberately #ifndef _WIN32 so the
# original x86/Win32 path remains unchanged.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
panel="$root/src/decompiled_cpp/ui/GameSetupPanel.cpp"
menu="$root/src/decompiled_cpp/ui/EditWindow.cpp"
header="$root/src/decompiled_cpp/ui/GameSetupPanel.h"

require() {
    local pattern="$1"
    local file="$2"
    grep -Fq "$pattern" "$file" || {
        echo "FAIL: missing '$pattern' in ${file#$root/}" >&2
        exit 1
    }
}

require 'void GameSetupPanel::hostHandlePointer(float display_x, float display_y, bool pressed)' "$panel"
require 'SDL3_DisplayToPrimaryCanvas(display_x, display_y,' "$panel"
require 'constexpr uint64_t kLobbyPressDurationMs = 150;' "$panel"
require 'SDL3_GameAudioPlayResource(0x5015);' "$panel"
require 'this->hostPressedUntilMs = SDL_GetTicks() + kLobbyPressDurationMs;' "$panel"
require 'pressed_control == HostLobbyControl::Exit ? 1 : 0' "$panel"
require 'pressed_control == HostLobbyControl::Search ? 1 : 0' "$panel"
require 'pressed_control == HostLobbyControl::Options ? 1 : 0' "$panel"
require 'constexpr HostGridLayout kHostGridLayouts[]' "$panel"
require '{"3x3", 3, 3}' "$panel"
require '{"2x2", 2, 2}' "$panel"
require '{"2x1", 2, 1}' "$panel"
require '{"3x1", 3, 1}' "$panel"
require '{"3x2", 3, 2}' "$panel"
require '_g_netman->m_playerRows = layout.display_columns;' "$panel"
require '_g_netman->m_playerCols = layout.display_rows;' "$panel"
require '_g_netman->m_playerSlotCount = layout.display_columns * layout.display_rows;' "$panel"
require 'const int layout_index = host_layout_at(canvas_x, canvas_y);' "$panel"
require 'host_apply_layout(*this, layout_index);' "$panel"
require 'bool host_draw_text(SDL_Renderer* renderer, int x, int y, const char* text,' "$panel"
require 'constexpr SDL_Color kOriginalListTextColor = {0x00, 0x5c, 0xff, 0xff};' "$panel"
require 'constexpr SDL_Color kOriginalSelectedListTextColor = {0xdc, 0x25, 0x25, 0xff};' "$panel"
require 'SDL_RenderFillRect(renderer, &pixel)' "$panel"
require 'case HostLobbyControl::Exit:' "$panel"
require 'g_editwindow_ptr->setState(7);' "$panel"
require 'case HostLobbyControl::Search:' "$panel"
require 'panel.hostSearchCompleted = true;' "$panel"
require 'case HostLobbyControl::Options:' "$panel"
require 'g_editwindow_ptr->setState(2);' "$panel"
require 'g_editwindow_ptr->setState(7);' "$panel"
require 'void hostHandlePointer(float display_x, float display_y, bool pressed);' "$header"
require 'this->pPanelB->hostHandlePointer(display_x, display_y, pressed);' "$menu"

# Confirm the routing remains inside the host-only compilation branch.
line=$(grep -n 'void GameSetupPanel::hostHandlePointer(float display_x, float display_y, bool pressed)' "$panel" | cut -d: -f1)
start=$(head -n "$line" "$panel" | grep -n '#ifndef _WIN32' | tail -n1 | cut -d: -f1)
end=$(tail -n "+$line" "$panel" | grep -n '#endif  // !_WIN32' | head -n1 | cut -d: -f1)
[[ -n "$start" && -n "$end" ]] || {
    echo 'FAIL: GameSetupPanel host input is not enclosed by #ifndef _WIN32' >&2
    exit 1
}

echo 'PASS: SDL mode-2 lobby input reaches the guarded 0x40A4E0 control adapter'
