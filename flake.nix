{
  description = "Lego Loco reverse engineering and Linux port development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
          overlays = [ ];
        };

        # MinGW-w64 cross-compilation toolchain (i686 Windows target, matches original binary)
        mingwToolchain = pkgs.pkgsCross.mingw32.stdenv.cc;

      in
      {
        # -----------------------------------------------------------------
        # Development shell — all tools needed for RE and compilation
        # -----------------------------------------------------------------
        devShells = {
          # Full development environment (default)
          default = pkgs.mkShell {
            name = "lego-loco-dev";

            buildInputs = with pkgs; [
              # ---- Nix tooling ----
              direnv

              # ---- C/C++ compilation ----
              gcc                    # native g++ for quick syntax checks
              gnumake                # Makefile build system
              cmake                  # CMake-based port build
              mingwToolchain         # i686-w64-mingw32 cross-compiler
                                     #   Provides: windows.h, ddraw.h, dsound.h, dplay.h
                                     #   Supports: __thiscall, __fastcall, __stdcall

              # ---- SDL3 host compatibility layer ----
              sdl3                    # host audio/video shims
              freetype                # native 14px Arial-compatible list text
              fontconfig              # resolves the host fallback face
              dejavu_fonts            # deterministic Fontconfig fallback font
              pkg-config              # Makefile dependency discovery

              # ---- Networking (for Linux port) ----
              dbus                   # Avahi DNS-SD through the system D-Bus
              sdl3-net               # TCP transport replacing DirectPlay

              # ---- GStreamer (for Linux port AVI playback) ----
              gst_all_1.gstreamer
              gst_all_1.gst-plugins-base
              gst_all_1.gst-plugins-good  # AVI demuxer
              gst_all_1.gst-libav         # Cinepak decoder for launch videos
              gst_all_1.gst-plugins-bad  # DVI ADPCM decoder for launch videos (legospin.avi)

              # ---- Reverse engineering tools ----
              ghidra                 # NSA's software reverse engineering suite
              binwalk                # firmware / binary analysis
              python3                # scripts in tools/ directory
              python3Packages.pytest  # component/integration test runner
              sway                    # swaymsg geometry for isolated GUI input
              python3Packages.fastapi # local autonomous RE dashboard API
              python3Packages.uvicorn # ASGI server for the dashboard daemon
              python3Packages.websockets # Uvicorn WebSocket implementation for live dashboard events
              python3Packages.httpx   # FastAPI dashboard integration tests
              nodejs_22                # React/Vite autonomous RE dashboard

              # ---- Wine (for running the original Windows binary) ----
              wine                   # Windows compatibility layer
              winetricks             # Wine helper for DLLs/components

              # ---- X11 / display tools (for running the game) ----
              xorg-server            # Xephyr + Xvfb (was xorg.xorgserver)
              xwd                    # X11 window dump (was xorg.xwd)
              scrot                  # screenshot tool
              imagemagick            # image conversion
              xdotool                # keyboard/mouse automation
              x11vnc                 # VNC server for remote display
              mesa                   # software rendering (llvmpipe)

              # ---- Archive / file tools ----
              unzip

              # ---- Optional: inih for INI parsing (used by port) ----
              # inih is vendored in third_party/, but available as:
              # inih
            ];

            # -----------------------------------------------------------------
            # Shell hook — prints available commands on entry
            # -----------------------------------------------------------------
            shellHook = ''
              echo ""
              echo "Compile decompiled C++ (object files only, no link):"
              echo "  make -C src/decompiled_cpp MINGW=1"
              echo "  make -C src/decompiled_cpp check"
              echo ""
            '';
          };

          # Minimal shell — just the cross-compiler + make (no GUI/Wine/Ghidra)
          build-only = pkgs.mkShell {
            name = "lego-loco-build";
            buildInputs = with pkgs; [
              gnumake
              mingwToolchain
              gcc  # for native fallback
            ];
            shellHook = ''
              echo "Lego Loco build-only shell (MinGW cross-compiler + GNU Make)"
              echo "  make -C src/decompiled_cpp MINGW=1"
            '';
          };
        };
      }
    );
}
