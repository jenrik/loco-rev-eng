#!/usr/bin/env bash
# Run Lego Loco on the host (not in container).
#
# Modes (controlled by DISPLAY env var):
#   DISPLAY unset → starts Xvfb :99 automatically (headless / software render)
#   DISPLAY=:0    → uses host X server (graphical, user can watch/play)
#
# The DDrawCompat ddraw.dll in the game dir crashes Wine; force builtin:
#   WINEDLLOVERRIDES=ddraw=b (set below, can be overridden)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

export WINEPREFIX="${WINEPREFIX:-$SCRIPT_DIR/wine-prefix}"
export WINEARCH=win32
export WINEDLLOVERRIDES="${WINEDLLOVERRIDES:-ddraw=b}"

GAME_DIR="$WINEPREFIX/drive_c/loco/Exe"
if [ ! -d "$GAME_DIR" ]; then
    echo "Error: Game not found at $GAME_DIR"
    echo "Run ./setup-game.sh first."
    exit 1
fi

XVFB_PID=""

if [ -z "${DISPLAY:-}" ]; then
    echo "No DISPLAY set — starting Xvfb :99 (software rendering, 16-bit)..."
    Xvfb :99 -screen 0 1024x768x16 &
    XVFB_PID=$!
    export DISPLAY=:99
    # Give Xvfb a moment to initialise
    sleep 0.5
    trap '[ -n "$XVFB_PID" ] && kill "$XVFB_PID" 2>/dev/null' EXIT
fi

echo "Launching Lego Loco on $DISPLAY..."
cd "$GAME_DIR"
WINEDEBUG=-all wine loco.exe "$@"
