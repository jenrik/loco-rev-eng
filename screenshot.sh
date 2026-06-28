#!/usr/bin/env bash
# Take a screenshot of the game display.
# Works both on the host (with Xvfb running) and inside the container via docker exec.
#
# Usage: screenshot.sh [output.png]
#   Output defaults to /tmp/loco-screenshot.png
#
# Environment:
#   DISPLAY — X display to capture (default :99)

set -euo pipefail

OUTPUT="${1:-/tmp/loco-screenshot.png}"
DISP="${DISPLAY:-:99}"

DISPLAY="$DISP" scrot "$OUTPUT"

echo "$OUTPUT"
