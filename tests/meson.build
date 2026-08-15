# ============================================================================
# Test suite — ported from the old Makefile's ~30 hand-written targets.
# Categorized per the migration plan:
#   - plain compiled unit tests
#   - symbol/linkage checks (nm/objdump against the real binary)
#   - source-grep regression checks (no binary needed)
#   - object-extraction isolation tests (link a single extracted .o)
#   - network-namespace / private-bus tests (wrapped via tests/env/*.sh)
# ============================================================================

# -Wno-cpp: Meson defines _FILE_OFFSET_BITS=64 by default, which on this
# NixOS glibc/gcc pairing triggers a "_FORTIFY_SOURCE requires -O" #warning
# at -O0 (the debug build default); the main binary's WARNFLAGS already
# carries the same suppression for the same reason.
test_warn_args = ['-Wall', '-Wextra', '-Werror', '-Wno-cpp']
env_dir = meson.current_source_dir() / 'env'

# ---------------------------------------------------------------------------
# Plain compiled unit tests
# ---------------------------------------------------------------------------

sdl3_net_protocol_test = executable('sdl3_net_protocol_test',
  'sdl3_net_protocol_test.cpp',
  objects: obj_sdl3_net_protocol,
  include_directories: inc_dirs,
  cpp_args: test_warn_args,
)
test('sdl3-net-protocol', sdl3_net_protocol_test)

sdl3_net_transport_test = executable('sdl3_net_transport_test',
  'sdl3_net_transport_test.cpp',
  objects: [obj_sdl3_net_protocol, obj_sdl3_net_transport],
  include_directories: inc_dirs,
  cpp_args: test_warn_args,
  dependencies: [sdl3_dep, sdl3_net_dep, threads_dep],
)
test('sdl3-net-transport', find_program('sdl3_net_transport_test.sh'),
  args: [sdl3_net_transport_test])

sdl3_net_runtime_test = executable('sdl3_net_runtime_test',
  'sdl3_net_runtime_test.cpp',
  objects: [obj_sdl3_net_protocol, obj_sdl3_net_transport, obj_sdl3_net_runtime],
  include_directories: inc_dirs,
  cpp_args: test_warn_args,
  dependencies: [sdl3_dep, sdl3_net_dep, threads_dep],
)
test('sdl3-net-runtime', sdl3_net_runtime_test)

network_discovery_test = executable('network_discovery_test',
  'network_discovery_test.cpp',
  objects: obj_network_discovery,
  include_directories: inc_dirs,
  cpp_args: test_warn_args,
)
test('network-discovery', network_discovery_test)

discovery_runtime_test = executable('discovery_runtime_test',
  'discovery_runtime_test.cpp',
  objects: [obj_network_discovery, obj_discovery_runtime],
  include_directories: inc_dirs,
  cpp_args: test_warn_args,
  dependencies: [threads_dep],
)
test('discovery-runtime', discovery_runtime_test)

# Avahi adapter regression runs against an isolated fake system service on a
# private dbus-daemon; it never modifies or depends on the host Avahi daemon.
avahi_dbus_discovery_test = executable('avahi_dbus_discovery_test',
  'avahi_dbus_discovery_test.cpp',
  objects: [obj_discovery_protocol, obj_avahi_dbus],
  include_directories: inc_dirs,
  cpp_args: test_warn_args,
  dependencies: [dbus_dep, threads_dep],
)
test('avahi-dbus-discovery', find_program(env_dir / 'with-private-dbus.sh'),
  args: [avahi_dbus_discovery_test])

# Run the embedded responder with sole control of UDP 5353 in a private
# user/network namespace; host Avahi/systemd-resolved remains untouched.
embedded_mdns_discovery_test = executable('embedded_mdns_discovery_test',
  'embedded_mdns_discovery_test.cpp',
  objects: [obj_discovery_protocol, obj_embedded_mdns],
  include_directories: inc_dirs,
  cpp_args: test_warn_args,
)
test('embedded-mdns-discovery', find_program(env_dir / 'with-dummy-netns.sh'),
  args: [embedded_mdns_discovery_test])

sdl3_net_discovery_transport_test = executable('sdl3_net_discovery_transport_test',
  'sdl3_net_discovery_transport_test.cpp',
  objects: [obj_discovery_protocol, obj_embedded_mdns, obj_sdl3_net_protocol, obj_sdl3_net_transport],
  include_directories: inc_dirs,
  cpp_args: test_warn_args,
  dependencies: [sdl3_dep, sdl3_net_dep, threads_dep],
)
test('sdl3-net-discovery-transport', find_program(env_dir / 'with-dummy-netns.sh'),
  args: [find_program('sdl3_net_discovery_transport_test.sh'), sdl3_net_discovery_transport_test])

# Asset archive regression test — validates real RFH indexing and Huf_Decode @ 0x45C830.
resource_archive_test = executable('resource_archive_test',
  'resource_archive_test.cpp',
  objects: [obj_resource_archive, obj_pe_string_table],
  include_directories: inc_dirs,
  cpp_args: test_warn_args,
)
test('resource-archive', resource_archive_test,
  args: [meson.project_source_root() / 'lego-loco-unpacked/art-res'],
  workdir: meson.project_source_root())

resource_manager_sdl3_test = executable('resource_manager_sdl3_test',
  'resource_manager_sdl3_test.cpp',
  objects: [obj_resource_archive, obj_pe_string_table, obj_resource_mgr_sdl3],
  include_directories: inc_dirs,
  cpp_args: test_warn_args,
  dependencies: [sdl3_dep],
)
test('resource-manager-sdl3', resource_manager_sdl3_test,
  env: ['SDL_VIDEODRIVER=dummy'],
  workdir: meson.project_source_root())

# Reachability proof for Entity::InitBase's host branch (core/GameObject.cpp):
# constructs a real Entity against the REAL resource_manager_sdl3.cpp bridge
# (not the persistence tests' fake GetById) to prove the host branch fires on
# a real archive resource without crashing on the original's null
# acquire_surface/release_surface vtable slots. See PROGRESS.md.
entity_host_resource_test = executable('entity_host_resource_test',
  'entity_host_resource_test.cpp',
  objects: [obj_resource_archive, obj_pe_string_table, obj_resource_mgr_sdl3,
            obj_game_object, obj_entity, obj_sprite_uipanel_adapter,
            obj_uipanel_surface_lifecycle],
  include_directories: inc_dirs,
  cpp_args: test_warn_args,
  dependencies: [sdl3_dep],
)
test('entity-host-resource', entity_host_resource_test,
  env: ['SDL_VIDEODRIVER=dummy'],
  workdir: meson.project_source_root())

dplay_config_test = executable('dplay_config_test',
  'dplay_config_test.cpp',
  include_directories: inc_dirs,
  cpp_args: test_warn_args,
)
test('dplay-config', dplay_config_test)

postbag_cleanup_test = executable('postbag_cleanup_test',
  'postbag_cleanup_test.cpp',
  objects: obj_postbag_cleanup,
  include_directories: inc_dirs,
  cpp_args: test_warn_args,
)
test('postbag-cleanup', postbag_cleanup_test)

# WIN32_StreamFile/WNDPROC_StreamBuf regression — write side (WriteChar/Flush/
# SetBuffer/dtor/CloseHandle) plus read side (Underflow/GetChar/ReadChar/Open)
# against real temp files. Was written but never registered as a meson test()
# target (found while extending it for Underflow()/Open() — see PROGRESS.md's
# "win32_stream.c removed (partial)" entry, 2026-08-10 update).
win32_stream_file_test = executable('win32_stream_file_test',
  'win32_stream_file_test.cpp',
  objects: [obj_win32_stream_file, obj_wndproc_stream_buf],
  include_directories: inc_dirs,
  cpp_args: test_warn_args,
)
test('win32-stream-file', win32_stream_file_test)

# SDL primary-target regression: validates the CGWND frame source reaches the window.
sdl3_primary_present_test = executable('sdl3_primary_present_test',
  'sdl3_primary_present_test.cpp',
  objects: [obj_sdl3_window, obj_sdl3_ddraw, obj_ddraw_globals, obj_sdl3_game_audio, obj_resource_mgr_sdl3,
            obj_resource_archive, obj_pe_string_table, obj_host_test_events],
  include_directories: inc_dirs,
  cpp_args: test_warn_args + ['-include', compat_h],
  dependencies: [sdl3_dep],
)
test('sdl3-primary-present', sdl3_primary_present_test, env: ['SDL_VIDEODRIVER=dummy'])

# Lock()/Unlock() 16bpp pixel-format adapter regression: guards the
# 32bpp-vs-16bpp mismatch documented in PROGRESS.md's DirectDraw-shim note.
sdl3_ddraw_pixel_format_test = executable('sdl3_ddraw_pixel_format_test',
  'sdl3_ddraw_pixel_format_test.cpp',
  objects: [obj_sdl3_window, obj_sdl3_ddraw, obj_ddraw_globals, obj_sdl3_game_audio, obj_resource_mgr_sdl3,
            obj_resource_archive, obj_pe_string_table, obj_host_test_events],
  include_directories: inc_dirs,
  cpp_args: test_warn_args + ['-include', compat_h],
  dependencies: [sdl3_dep],
)
test('sdl3-ddraw-pixel-format', sdl3_ddraw_pixel_format_test, env: ['SDL_VIDEODRIVER=dummy'])

# DDSURFACEDESC field-order regression: guards the dwWidth/dwHeight transposition
# landmine platform/sdl3_types.h used to have (see NOTE-directx-sdk.md / PROGRESS.md).
sdl3_ddraw_surfacedesc_test = executable('sdl3_ddraw_surfacedesc_test',
  'sdl3_ddraw_surfacedesc_test.cpp',
  objects: [obj_sdl3_window, obj_sdl3_ddraw, obj_ddraw_globals, obj_sdl3_game_audio, obj_resource_mgr_sdl3,
            obj_resource_archive, obj_pe_string_table, obj_host_test_events],
  include_directories: inc_dirs,
  cpp_args: test_warn_args + ['-include', compat_h],
  dependencies: [sdl3_dep],
)
test('sdl3-ddraw-surfacedesc', sdl3_ddraw_surfacedesc_test, env: ['SDL_VIDEODRIVER=dummy'])

# IDirectDrawPalette/IDirectDrawClipper lifecycle regression (DirectDraw shim Phase 4).
sdl3_ddraw_palette_clipper_test = executable('sdl3_ddraw_palette_clipper_test',
  'sdl3_ddraw_palette_clipper_test.cpp',
  objects: [obj_sdl3_window, obj_sdl3_ddraw, obj_ddraw_globals, obj_sdl3_game_audio, obj_resource_mgr_sdl3,
            obj_resource_archive, obj_pe_string_table, obj_host_test_events],
  include_directories: inc_dirs,
  cpp_args: test_warn_args + ['-include', compat_h],
  dependencies: [sdl3_dep],
)
test('sdl3-ddraw-palette-clipper', sdl3_ddraw_palette_clipper_test, env: ['SDL_VIDEODRIVER=dummy'])

# Mode-2 EditWindow::render regression: recovered backdrop resources reach the SDL primary target.
mode2_menu_backdrop_test = executable('mode2_menu_backdrop_test',
  'mode2_menu_backdrop_test.cpp',
  objects: [obj_resource_archive, obj_pe_string_table, obj_resource_mgr_sdl3,
            obj_sdl3_window, obj_sdl3_ddraw, obj_ddraw_globals, obj_sdl3_game_audio, obj_host_test_events],
  include_directories: inc_dirs,
  cpp_args: test_warn_args + ['-include', compat_h],
  dependencies: [sdl3_dep],
)
test('mode2-menu-backdrop', mode2_menu_backdrop_test,
  env: ['SDL_VIDEODRIVER=dummy'], workdir: meson.project_source_root())

# Mode-2 multiplayer GameSetupPanel host-composition regression.
mode2_multiplayer_menu_test = executable('mode2_multiplayer_menu_test',
  'mode2_multiplayer_menu_test.cpp',
  objects: [obj_resource_archive, obj_pe_string_table, obj_resource_mgr_sdl3,
            obj_sdl3_window, obj_sdl3_ddraw, obj_ddraw_globals, obj_sdl3_game_audio, obj_host_test_events],
  include_directories: inc_dirs,
  cpp_args: test_warn_args + ['-include', compat_h],
  dependencies: [sdl3_dep],
)
test('mode2-multiplayer-menu', mode2_multiplayer_menu_test,
  env: ['SDL_VIDEODRIVER=dummy'], workdir: meson.project_source_root())

# Host sound regression: mode 2 preloads 0x5015; mode 10 plays 0x5026.
sdl3_game_audio_test = executable('sdl3_game_audio_test',
  'sdl3_game_audio_test.cpp',
  objects: [obj_resource_archive, obj_pe_string_table, obj_resource_mgr_sdl3,
            obj_host_test_events, obj_sdl3_game_audio],
  include_directories: inc_dirs,
  cpp_args: test_warn_args,
  dependencies: [sdl3_dep],
)
test('sdl3-game-audio', sdl3_game_audio_test,
  env: ['SDL_AUDIODRIVER=dummy'], workdir: meson.project_source_root())

# DirectSound bridge: GameAudio::Init() through the real AudioDirectSoundDevice/
# AudioDirectSoundBuffer interface down to the SDL3 backend -- otherwise
# entirely unexercised (the real host bootstrap path defers device init to
# the separate SDL3_GameAudio subsystem instead; see the test file header).
sdl3_dsound_adapter_test = executable('sdl3_dsound_adapter_test',
  'sdl3_dsound_adapter_test.cpp',
  objects: [obj_gameaudio, obj_audiochannel, obj_sdl3_dsound, obj_sdl3_dsound_adapter],
  include_directories: inc_dirs,
  cpp_args: test_warn_args + ['-include', compat_h],
  dependencies: [sdl3_dep],
)
test('sdl3-dsound-adapter', sdl3_dsound_adapter_test,
  env: ['SDL_AUDIODRIVER=dummy'], workdir: meson.project_source_root())

# Original MCI launch order recovered from 0x421EB0 / 0x420F7F. Header-only.
intro_video_sequence_test = executable('intro_video_sequence_test',
  'intro_video_sequence_test.cpp',
  include_directories: inc_dirs,
  cpp_args: test_warn_args,
)
test('intro-video-sequence', intro_video_sequence_test)

# SDL3 timer safety regression: rapid SetTimer/KillTimer cycles under thread contention.
sdl3_timer_stress_test = executable('sdl3_timer_stress_test',
  'sdl3_timer_stress_test.cpp',
  objects: [obj_sdl3_window, obj_sdl3_game_audio, obj_resource_mgr_sdl3,
            obj_resource_archive, obj_pe_string_table, obj_host_test_events],
  include_directories: inc_dirs,
  cpp_args: test_warn_args + ['-include', compat_h],
  dependencies: [sdl3_dep],
)
test('sdl3-timer-stress', sdl3_timer_stress_test, env: ['SDL_VIDEODRIVER=dummy'])

# ---------------------------------------------------------------------------
# Object-extraction isolation tests — link one hand-picked object from the
# main binary; some deliberately omit --unresolved-symbols=ignore-all so the
# link fails loudly if the isolated file references anything the test does
# not truthfully provide.
# ---------------------------------------------------------------------------

# CGWND_EnterMode3(2) safe early return and symbol ownership regression.
cgwnd_entermode3_test = executable('cgwnd_entermode3_test',
  'cgwnd_entermode3_test.cpp',
  objects: obj_cgwnd,
  include_directories: inc_dirs,
  cpp_args: test_warn_args + ['-include', compat_h],
  link_args: ['-no-pie', '-Wl,--unresolved-symbols=ignore-all'],
)
test('cgwnd-entermode3', cgwnd_entermode3_test)

# Regression for lego_loco-3.core: GameLoop_Setup must create both host
# mode-3 frame dependencies before their unconditional first-frame dispatch.
host_mode3_bootstrap_test = executable('host_mode3_bootstrap_test',
  'host_mode3_bootstrap_test.cpp',
  objects: [obj_sdl3_town_mode3, obj_game_view, obj_panel, obj_ddraw,
            obj_ui_utils, obj_game_object, obj_entity, obj_sdl3_window],
  include_directories: inc_dirs,
  cpp_args: test_warn_args + ['-include', compat_h],
  link_args: ['-no-pie', '-Wl,--unresolved-symbols=ignore-all'],
)
test('host-mode3-bootstrap', host_mode3_bootstrap_test)

persistence_objs = [obj_input_mgr, obj_resdata_save, obj_persistence_adapt, obj_config_ini]
persistence_cone = [obj_track_pos, obj_game_object, obj_entity, obj_building_mgr_grp,
                     obj_resdata_vehicle, obj_game_vehicle, obj_help_page_node, obj_building,
                     obj_sprite_uipanel_adapter, obj_uipanel_surface_lifecycle]

# Canonical InputMgr regression: ctor 0x41D250 / dtor body 0x41D2D0 / embedded
# entity-collection ops / ResetWorldState 0x41E100. Links the real InputMgr.o;
# no --unresolved-symbols=ignore-all: the link fails loudly if InputMgr.o
# references any symbol the test does not truthfully provide.
#
# obj_config_ini (game/ConfigIni.cpp) is included above because
# INPUT_LoadTimeEvents/INPUT_DiscoverEasterEgg call the real
# Config_GetIniString/Config_WriteInt (0x452D80/0x452DB0) — a genuinely
# self-contained leaf translation unit (no external deps beyond <cstring>),
# unlike game/PlayerConfig.cpp or core/Game.cpp, both of which would pull in
# much larger dependency graphs if extracted whole (see
# tests/persistence_fixtures.h's comments next to the Game_IsPositionBetween/
# UI_CreateMessageBox/PlayerConfig_Ctor/PlayerConfig_Dtor loud fixtures for
# why those stay fixtures instead).
inputmgr_canonical_test = executable('inputmgr_canonical_test',
  'inputmgr_canonical_test.cpp',
  objects: persistence_objs + persistence_cone,
  include_directories: inc_dirs + [include_directories('.')],
  cpp_args: test_warn_args + ['-include', compat_h],
)
test('inputmgr-canonical', inputmgr_canonical_test)

# Persistence adapter strict parse/write regressions (shipped fixtures).
persistence_adapter_test = executable('persistence_adapter_test',
  'persistence_adapter_test.cpp',
  objects: obj_persistence_adapt,
  include_directories: inc_dirs + [include_directories('.')],
  cpp_args: test_warn_args + ['-include', compat_h],
)
test('persistence-adapter', persistence_adapter_test, workdir: meson.project_source_root())

# INPUT_* world new/load/save + typed callees regressions (shipped fixtures).
input_world_test = executable('input_world_test',
  'input_world_test.cpp',
  objects: persistence_objs + persistence_cone,
  include_directories: inc_dirs + [include_directories('.')],
  cpp_args: test_warn_args + ['-include', compat_h],
)
test('input-world', input_world_test, workdir: meson.project_source_root())

# INPUT_PlaceObject reachability: links the real InputMgr.o AND the real
# resource_manager_sdl3.cpp bridge (unlike the two tests above) to construct
# a real placed entity from a real archive resource, not just prove the
# dispatcher survives a null lookup.
input_place_object_test = executable('input_place_object_test',
  'input_place_object_test.cpp',
  objects: persistence_objs + persistence_cone +
           [obj_resource_archive, obj_pe_string_table, obj_resource_mgr_sdl3,
            obj_town_backdrop_adapter],
  include_directories: inc_dirs + [include_directories('.')],
  cpp_args: test_warn_args + ['-include', compat_h],
  dependencies: [sdl3_dep],
)
test('input-place-object', input_place_object_test,
  env: ['SDL_VIDEODRIVER=dummy'],
  workdir: meson.project_source_root())

# ---------------------------------------------------------------------------
# Symbol/linkage checks against the real lego_loco binary (nm/objdump).
# ---------------------------------------------------------------------------

test('uipanel-surface-linkage', find_program('uipanel_surface_linkage_test.sh'), args: [lego_loco])
test('uipanel-surface-construction', find_program('uipanel_surface_construction_test.sh'), args: [lego_loco])
test('host-menu-renderer-linkage', find_program('host_menu_renderer_linkage_test.sh'), args: [lego_loco])
test('host-intro-video-linkage', find_program('host_intro_video_linkage_test.sh'), args: [lego_loco])

# ---------------------------------------------------------------------------
# Source-grep regression checks — no binary needed, self-locate their root.
# ---------------------------------------------------------------------------

test('host-main-menu-accept', find_program('host_main_menu_accept_test.sh'))
test('host-multiplayer-selector', find_program('host_multiplayer_selector_test.sh'))
test('host-multiplayer-menu-input', find_program('host_multiplayer_menu_input_test.sh'))

# ---------------------------------------------------------------------------
# menu-sprite-viewer / test-menu-sprite-viewer: NOT ported. tools/menu_sprite_
# viewer.cpp does not exist anywhere in the repo or git history — this target
# was already broken in the old Makefile before this migration.
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Integration GUI suite — isolated Wayland/Sway compositor flows. Wrapping it
# as a real meson test() (rather than a bare pytest invocation) means
# MESON_BUILD_ROOT is set automatically, which conftest.py's build_game
# fixture and test_game_gui.py's sdl3_net_transport_test path both rely on
# instead of a hardcoded build directory name.
# ---------------------------------------------------------------------------

python3 = find_program('python3')
test('integration', python3,
  args: ['-m', 'pytest', '-v', '-m', 'integration and gui', 'tests/integration'],
  workdir: meson.project_source_root(),
  env: ['LEGO_LOCO_BUILD_ROOT=' + meson.global_build_root()],
  suite: 'integration',
  is_parallel: false,
  timeout: 300,
)

