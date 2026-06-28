#!/usr/bin/env bash
# Send keyboard or mouse input to the game.
# Works both on the host (with Xvfb running) and inside the container via docker exec.
#
# Usage:
#   send-input.sh key <keyname>        e.g. key Return, key Escape, key space
#   send-input.sh click <x> <y>        left-click at screen coordinates
#   send-input.sh rclick <x> <y>       right-click at screen coordinates
#   send-input.sh move <x> <y>         move mouse without clicking
#   send-input.sh type <text>          type a string
#
# Environment:
#   DISPLAY — X display to target (default :99)

set -euo pipefail

export DISPLAY="${DISPLAY:-:99}"

if [ $# -lt 1 ]; then
    echo "Usage: $0 key <keyname> | click <x> <y> | rclick <x> <y> | move <x> <y> | type <text>" >&2
    exit 1
fi

CMD="$1"
shift

case "$CMD" in
    key)
        xdotool key "$@"
        ;;
    click)
        xdotool mousemove "$1" "$2" click 1
        ;;
    rclick)
        xdotool mousemove "$1" "$2" click 3
        ;;
    move)
        xdotool mousemove "$1" "$2"
        ;;
    type)
        xdotool type "$@"
        ;;
    *)
        echo "Unknown command: $CMD" >&2
        echo "Usage: $0 key <keyname> | click <x> <y> | rclick <x> <y> | move <x> <y> | type <text>" >&2
        exit 1
        ;;
esac
