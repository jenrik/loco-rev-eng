project('lego-loco', ['c', 'cpp'],
  version: '0.1.0',
  meson_version: '>=1.6.0',
  default_options: ['cpp_std=c++17', 'c_std=c11', 'warning_level=0'],
)

# ============================================================================
# One unified source tree, rooted directly at the repo root. Decompiled
# classes and host/SDL3 replacements live side by side per subsystem
# directory; host-only code is guarded with #ifndef _WIN32 so the same file
# list builds for both the native Linux target and the MinGW typecheck
# target (see cross/mingw32-typecheck.txt) — under MinGW, guarded files
# compile down to empty translation units for free.
# ============================================================================

subsys_dirs = [
  'core', 'game', 'graphics', 'audio', 'input', 'network', 'resources',
  'town', 'ui', 'world', 'shared', 'stubs', 'platform',
]

# Configure-time glob per subsystem directory. Meson discourages globbing
# because it doesn't trigger reconfigure automatically — this project adds a
# new decompiled file most sessions, so this trades strict Meson idiom for
# matching the previous Makefile's $(wildcard) auto-discovery UX. Run
# `meson setup --reconfigure build` after adding a new source file.
#
# Output is piped through `sort` for reproducible link order (LINK-001:
# a prior --allow-multiple-definition ODR violation between link_stubs.cpp
# and stubs_impl.cpp made the binary's behavior depend on unstable
# filesystem directory-entry order; that flag is gone and every duplicate
# symbol resolved, but sorting the source list stays correct practice
# regardless).
find_prog = find_program('find')

dcp_sources = []
foreach d : subsys_dirs
  result = run_command('sh', '-c', 'find "$1" -maxdepth 1 -name "$2" | sort', 'sh', d, '*.cpp', check: true)
  listing = result.stdout().strip()
  if listing != ''
    foreach f : listing.split('\n')
      dcp_sources += files(f)
    endforeach
  endif
endforeach

# native/*.c builds unconditionally — the former NATIVE_BROKEN exclusion list
# was retired; every entry was triaged (deleted as superseded, deleted with a
# PROGRESS.md TODO, or un-excluded after confirming it already compiles).
native_result = run_command('sh', '-c', 'find "$1" -maxdepth 1 -name "$2" | sort', 'sh', 'native', '*.c', check: true)
native_listing = native_result.stdout().strip()
native_sources = []
if native_listing != ''
  foreach f : native_listing.split('\n')
    native_sources += files(f)
  endforeach
endif

# platform/ additionally holds a couple of .c files (stub_func.c) alongside
# main.cpp.
platform_c_result = run_command('sh', '-c', 'find "$1" -maxdepth 1 -name "$2" | sort', 'sh', 'platform', '*.c', check: true)
platform_c_listing = platform_c_result.stdout().strip()
platform_c_sources = []
if platform_c_listing != ''
  foreach f : platform_c_listing.split('\n')
    platform_c_sources += files(f)
  endforeach
endif

# ============================================================================
# Dependencies — resolved through the Nix dev shell's propagated pkg-config
# path / cc-wrapper flags. No manual /nix/store globbing, no fallback shells.
# Skipped entirely under the MinGW cross build: no cross-compiled SDL3/
# SDL3_net/GStreamer/dbus exist in this Nix setup, and none are needed there
# anyway — every host-only file that would use them is #ifndef _WIN32-guarded
# down to an empty translation unit under MinGW.
# ============================================================================

host_ui_font_file = ''
if not meson.is_cross_build()
  sdl3_dep      = dependency('sdl3', include_type: 'system')
  sdl3_net_dep  = dependency('sdl3-net', include_type: 'system')
  gst_app_dep   = dependency('gstreamer-app-1.0', include_type: 'system')
  gst_video_dep = dependency('gstreamer-video-1.0', include_type: 'system')
  freetype_dep  = dependency('freetype2', include_type: 'system')
  dbus_dep      = dependency('dbus-1', include_type: 'system')
  threads_dep   = dependency('threads')

  # GameSetupPanel list text follows ResourceManager_Init (0x44611A): 14px
  # Arial, weight 700. Fontconfig supplies the host-compatible face.
  fc_match = find_program('fc-match', required: false)
  if fc_match.found()
    fc_result = run_command(fc_match, '-f', '%{file}', 'Arial', check: false)
    if fc_result.returncode() == 0
      host_ui_font_file = fc_result.stdout().strip()
    endif
  endif
endif

# ============================================================================
# Anti-pattern compiler flags (see AGENTS.md "Fix anti-patterns on sight")
# Three tiers, selected via -Dstrict=0|1|2 (default 0):
#   0  Tier 1 — always-on ABI/layout correctness checks
#   1  Tier 1+2 — calling-convention and cv/null checks
#   2  Tier 1+3 — old-style-cast and Effective C++ audits
# ============================================================================

warnflags = [
  '-Werror=delete-non-virtual-dtor', '-Werror=non-virtual-dtor',
  '-Werror=class-memaccess', '-Werror=cast-function-type',
  '-Werror=return-type', '-Werror=strict-aliasing', '-Werror=write-strings',
  '-Werror=narrowing', '-Werror=reorder', '-Werror=overloaded-virtual',
  '-Werror=pmf-conversions', '-Werror=invalid-offsetof',
  '-Werror=missing-field-initializers', '-Werror=suggest-override',
  '-Werror=subobject-linkage', '-Werror=cast-align',
  '-Werror=int-to-pointer-cast', '-Werror=pointer-arith',
  '-Wno-unknown-pragmas', '-Wno-cpp', '-Wno-unused',
  '-Wno-unused-parameter', '-Wno-attributes',
]

strict = get_option('strict')
if strict >= 1
  warnflags += ['-Wattributes', '-Werror=attributes', '-Werror=cast-qual', '-Werror=zero-as-null-pointer-constant']
endif
if strict >= 2
  warnflags += ['-Werror=old-style-cast', '-Weffc++', '-Werror=missing-declarations']
endif

# Applied project-wide (not per-target) so every target — including test
# executables added later via subdir('tests') — gets the anti-pattern
# allowlist by construction. A per-target cpp_args/c_args list can be
# forgotten on a new target with no error; add_project_arguments() cannot.
add_project_arguments(warnflags, language: 'cpp')
add_project_arguments(warnflags, language: 'c')

# ============================================================================
# Include dirs — every subsystem directory is a global include path so bare
# filenames (#include "sdl3_ddraw.h") resolve regardless of which directory
# the includer lives in, and subsystem-prefixed includes
# (#include "game/Train.h") resolve via the repo root itself.
# ============================================================================

inc_dirs = [include_directories('.')]
foreach d : subsys_dirs + ['native']
  inc_dirs += include_directories(d)
endforeach

compat_h = meson.current_source_dir() / 'stubs' / 'compat.h'

common_cpp_args = [
  '-fpermissive',
  '-include', compat_h,
  '-DLOCO_HOST_UI_FONT_FILE="' + host_ui_font_file + '"',
]
# native/*.c and platform/*.c are compiled as C++ (not C), matching the
# original Makefile's use of $(CXX) for .c sources: callers declare these
# functions with C++ linkage (no `extern "C"`), so both sides must be
# name-mangled the same way for the link to resolve.
common_c_args = ['-fpermissive', '-x', 'c++', '-include', compat_h]

if meson.is_cross_build()
  # ==========================================================================
  # MinGW typecheck build — compile-only, no link, no external dependencies.
  # Equivalent of the old `make -C src/decompiled_cpp MINGW=1`: that build
  # never included native/, stubs/, or sdl3_shims/ (main.cpp, stub_func.c)
  # either, so this mirrors the same scope — only the real decompiled
  # classes get typechecked against real Windows headers. Under this cross
  # file, _WIN32 is defined, so every #ifndef _WIN32-guarded host file in
  # these directories (e.g. graphics/sdl3_ddraw.cpp) compiles to an empty
  # translation unit for free.
  # ==========================================================================
  typecheck_dirs = [
    'core', 'game', 'graphics', 'audio', 'input', 'network', 'resources',
    'town', 'ui', 'world', 'shared',
  ]
  typecheck_sources = []
  foreach d : typecheck_dirs
    result = run_command('sh', '-c', 'find "$1" -maxdepth 1 -name "$2" | sort', 'sh', d, '*.cpp', check: true)
    listing = result.stdout().strip()
    if listing != ''
      foreach f : listing.split('\n')
        typecheck_sources += files(f)
      endforeach
    endif
  endforeach

  static_library('lego_loco_typecheck', typecheck_sources,
    include_directories: inc_dirs,
    cpp_args: common_cpp_args,
    build_by_default: true,
  )
else
  # ============================================================================
  # Main binary
  # ============================================================================
  # --allow-multiple-definition removed (LINK-001): every duplicate-symbol
  # collision it was masking is resolved (see PROGRESS.md). TODO (LINK-001,
  # remaining): remove --unresolved-symbols=ignore-all — this still masks
  # missing-implementation defects (a much larger, separate pass: doing so
  # requires resolving every currently-dangling undefined symbol first).

  main_link_args = ['-no-pie']
  if get_option('unresolved_ignore')
    main_link_args += ['-Wl,--unresolved-symbols=ignore-all']
  endif

  lego_loco = executable('lego_loco',
    dcp_sources + native_sources + platform_c_sources,
    include_directories: inc_dirs,
    cpp_args: common_cpp_args,
    c_args: common_c_args,
    link_args: main_link_args,
    dependencies: [sdl3_dep, sdl3_net_dep, gst_app_dep, gst_video_dep, freetype_dep, dbus_dep, threads_dep],
    install: false,
  )

  # Objects extracted from the main binary's already-compiled translation
  # units, reused by isolation-style tests instead of recompiling the same
  # source a second time.
  obj_cgwnd            = lego_loco.extract_objects('core/CGWND.cpp')
  obj_sdl3_town_mode3   = lego_loco.extract_objects('town/sdl3_town_mode3.cpp')
  obj_game_object       = lego_loco.extract_objects('core/GameObject.cpp')
  obj_entity            = lego_loco.extract_objects('core/Entity.cpp')
  # GameView/Panel/DDRAW_Building — needed by host_mode3_bootstrap_test now
  # that BootstrapTownMode3Objects() placement-new constructs both
  # singletons for real (BUG-mode3-input-processing-crashes.md) instead of
  # leaving their storage as raw zeroed bytes.
  obj_game_view         = lego_loco.extract_objects('core/GameView.cpp')
  obj_panel             = lego_loco.extract_objects('game/Panel.cpp')
  obj_ddraw             = lego_loco.extract_objects('graphics/DDRAW.cpp')
  # DDRAW.cpp now defines the real UI_Manager singleton backing
  # `g_tooltip_mgr` (0x4FD220 — see that file's doc comment), so any test
  # that links obj_ddraw needs UI_Utils.o too: its static initializer
  # unconditionally constructs a UI_Manager, matching the original CRT
  # $XCU static-init entry at 0x45C680.
  obj_ui_utils          = lego_loco.extract_objects('ui/UI_Utils.cpp')
  obj_building_mgr_grp  = lego_loco.extract_objects('core/BuildingMgrObjectGroup.cpp')
  obj_resdata_vehicle   = lego_loco.extract_objects('game/ResdataGameVehicle.cpp')
  obj_game_vehicle      = lego_loco.extract_objects('game/GameVehicle.cpp')
  obj_help_page_node    = lego_loco.extract_objects('ui/HelpPageNode.cpp')
  obj_building          = lego_loco.extract_objects('game/Building.cpp')
  obj_input_mgr         = lego_loco.extract_objects('input/InputMgr.cpp')
  obj_track_pos         = lego_loco.extract_objects('game/TrackPos.cpp')
  obj_resdata_save      = lego_loco.extract_objects('resources/ResDataSave.cpp')
  obj_persistence_adapt = lego_loco.extract_objects('input/PersistenceAdapter.cpp')
  obj_postbag_cleanup   = lego_loco.extract_objects('network/PostBagCleanup.cpp')

  # Shim/subsystem objects reused by tests instead of recompiling them again.
  obj_sdl3_net_protocol   = lego_loco.extract_objects('network/sdl3_net_protocol.cpp')
  obj_sdl3_net_transport  = lego_loco.extract_objects('network/sdl3_net_transport.cpp')
  obj_sdl3_net_runtime    = lego_loco.extract_objects('network/sdl3_net_runtime.cpp')
  obj_network_discovery   = lego_loco.extract_objects('network/network_discovery.cpp')
  obj_discovery_runtime   = lego_loco.extract_objects('network/discovery_runtime.cpp')
  obj_discovery_protocol  = lego_loco.extract_objects('network/network_discovery_protocol.cpp')
  obj_avahi_dbus          = lego_loco.extract_objects('network/avahi_dbus_discovery.cpp')
  obj_embedded_mdns       = lego_loco.extract_objects('network/embedded_mdns_discovery.cpp')
  obj_resource_archive    = lego_loco.extract_objects('resources/resource_archive.cpp')
  obj_pe_string_table     = lego_loco.extract_objects('resources/pe_string_table.cpp')
  obj_resource_mgr_sdl3   = lego_loco.extract_objects('resources/resource_manager_sdl3.cpp')
  obj_sdl3_window         = lego_loco.extract_objects('graphics/sdl3_window.cpp')
  obj_sdl3_ddraw          = lego_loco.extract_objects('graphics/sdl3_ddraw.cpp')
  obj_sdl3_game_audio     = lego_loco.extract_objects('audio/sdl3_game_audio.cpp')
  obj_host_test_events    = lego_loco.extract_objects('stubs/host_test_events.cpp')
  obj_win32_stream_file   = lego_loco.extract_objects('resources/Win32StreamFile.cpp')
  obj_wndproc_stream_buf  = lego_loco.extract_objects('resources/WndProcStreamBuf.cpp')

  subdir('tests')
endif
