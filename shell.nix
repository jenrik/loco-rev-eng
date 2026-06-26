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
    xorg-server  # Provides Xephyr
    ghidra
    python3
  ];
}
