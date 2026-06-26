#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

export WINEPREFIX="$SCRIPT_DIR/wine-prefix"
export WINEARCH=win32

GAME_DIR="$WINEPREFIX/drive_c/loco/Exe"
if [ ! -d "$GAME_DIR" ]; then
    echo "Error: Game not found at $GAME_DIR"
    echo "Run ./setup-game.sh first to install the game."
    exit 1
fi

echo "Launching Lego Loco..."
cd "$GAME_DIR"
wine loco.exe "$@"
