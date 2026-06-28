#!/usr/bin/env bash
# Container entrypoint — starts Xvfb, optionally VNC, then the game.
#
# Environment variables:
#   VNC_ENABLED=1       Start x11vnc so the user can connect on port 5900
#   VNC_PASSWORD=<pw>   VNC password (optional; unset = no password)
#   LOCO_AUTOSTART=0    Don't start the game automatically (use docker exec instead)
set -euo pipefail

DISPLAY="${DISPLAY:-:99}"
WINEPREFIX="${WINEPREFIX:-/wine-prefix}"
GAME_EXE="$WINEPREFIX/drive_c/loco/Exe/loco.exe"

# Validate game is present
if [ ! -f "$GAME_EXE" ]; then
    echo "ERROR: Game not found at $GAME_EXE"
    echo "Mount the wine-prefix directory: -v ./wine-prefix:/wine-prefix"
    exit 1
fi

# Start Xvfb — 16-bit depth so DirectDraw mode changes succeed
Xvfb "$DISPLAY" -screen 0 1024x768x16 &
XVFB_PID=$!
export DISPLAY
echo "Xvfb started on $DISPLAY (PID $XVFB_PID)"
sleep 0.5

# Optional VNC for user play
if [ "${VNC_ENABLED:-0}" = "1" ]; then
    PASSWORD_ARGS=()
    if [ -n "${VNC_PASSWORD:-}" ]; then
        PASSWORD_ARGS=(-passwd "$VNC_PASSWORD")
    fi
    x11vnc -display "$DISPLAY" -forever -bg -rfbport 5900 \
        "${PASSWORD_ARGS[@]}" -noxdamage -quiet -logfile /tmp/x11vnc.log
    echo "VNC started on port 5900 (connect with any VNC viewer)"
fi

# Start game unless explicitly disabled
if [ "${LOCO_AUTOSTART:-1}" = "1" ]; then
    cd "$WINEPREFIX/drive_c/loco/Exe"
    WINEDEBUG=-all wine loco.exe &
    echo "Lego Loco started (PID $!)"
fi

# Keep container alive as long as Xvfb runs
wait "$XVFB_PID"
