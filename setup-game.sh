#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DISC_DIR="$SCRIPT_DIR/lego-loco-unpacked"

export WINEPREFIX="$SCRIPT_DIR/wine-prefix"
export WINEARCH=win32

# Initialize wine prefix if it doesn't exist
if [ ! -d "$WINEPREFIX" ]; then
    echo "Initializing Wine prefix at $WINEPREFIX..."
    wineboot --init
fi

# The 16-bit InstallShield installer crashes under Wine's winevdm,
# so we manually set up the game instead.

# Create the game install directory
INSTALL_DIR="$WINEPREFIX/drive_c/loco"
echo "Installing game files to $INSTALL_DIR..."
mkdir -p "$INSTALL_DIR"

# Copy game files from disc
cp -r "$DISC_DIR/Exe" "$INSTALL_DIR/"
cp -r "$DISC_DIR/art-res" "$INSTALL_DIR/"

# Update LEGO.INI to point to the install location (C:\loco\...)
cat > "$INSTALL_DIR/Exe/LEGO.INI" <<'EOF'
//LEGO ini file

[DIRECTORIES]
Res=c:\loco\art-res
ResFile=c:\loco\art-res\resource.rfh
exe=loco.exe

[Video]
Dir=c:\loco\Art-res\Video\locointr.avi
EOF

# Apply binary patches
"$SCRIPT_DIR/patches/skip-color-depth-check.sh" "$INSTALL_DIR/Exe/loco.exe"

echo "Game installed to $INSTALL_DIR"
echo "Run ./run-game.sh to launch."
