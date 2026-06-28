{
  pkgs ? import <nixpkgs> { },
}:

pkgs.mkShell {
  buildInputs = with pkgs; [
    direnv
    binwalk
    wine
    winetricks
    unzip
    xorg-server  # Provides Xephyr + Xvfb
    xorg.xwd     # X11 window dump (screenshots without imagemagick)
    scrot        # screenshot tool
    imagemagick  # convert/import for screenshots
    xdotool      # keyboard/mouse automation for agent use
    mesa         # software rendering (llvmpipe)
    ghidra
    python3
  ];
}
