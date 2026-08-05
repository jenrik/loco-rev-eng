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
              meson                  # build system
              ninja                  # Meson's backend
              mingwToolchain         # i686-w64-mingw32 cross-compiler
                                     #   Provides: windows.h, ddraw.h, dsound.h, dplay.h
                                     #   Supports: __thiscall, __fastcall, __stdcall

              # ---- SDL3 host compatibility layer ----
              sdl3                    # host audio/video shims
              freetype                # native 14px Arial-compatible list text
              fontconfig              # resolves the host fallback face
              dejavu_fonts            # deterministic Fontconfig fallback font
              pkg-config              # Meson dependency discovery

              # ---- Networking (for Linux port) ----
              dbus                   # Avahi DNS-SD through the system D-Bus
              sdl3-net               # TCP transport replacing DirectPlay

              # ---- GStreamer (for Linux port AVI playback) ----
              gst_all_1.gstreamer
              gst_all_1.gst-plugins-base
              gst_all_1.gst-plugins-good  # AVI demuxer
              gst_all_1.gst-libav         # Cinepak decoder for launch videos
              gst_all_1.gst-plugins-bad  # DVI ADPCM decoder for launch videos (legospin.avi)

              # ---- Reverse engineering and debugging tools ----
              ghidra                 # NSA's software reverse engineering suite
              binwalk                # firmware / binary analysis
              gdb                    # inspect live processes and ELF core dumps
              valgrind               # native memory and concurrency diagnostics
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
              echo "Build lego_loco:"
              echo "  meson setup build && meson compile -C build"
              echo "MinGW cross-compile typecheck (decompiled classes only, not linked):"
              echo "  meson setup build-mingw --cross-file cross/mingw32-typecheck.txt"
              echo "  ninja -C build-mingw -k 0"
              echo ""
            '';
          };

          # Minimal shell — just the cross-compiler + build tools (no GUI/Wine/Ghidra)
          build-only = pkgs.mkShell {
            name = "lego-loco-build";
            buildInputs = with pkgs; [
              meson
              ninja
              mingwToolchain
              gcc  # for native fallback
            ];
            shellHook = ''
              echo "Lego Loco build-only shell (MinGW cross-compiler + Meson)"
              echo "  meson setup build-mingw --cross-file cross/mingw32-typecheck.txt"
            '';
          };
        };
      }
    );
}
