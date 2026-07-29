# ============================================================================
# Lego Loco -- SDL3 Native Port -- Unified Build System
# ============================================================================
#
# Single command:  make
#
# Targets:
#   make              Build the lego_loco binary (default)
#   make build        Build the lego_loco binary (alias)
#   make run          Build and run
#   make clean        Remove all build artifacts
#   make check        Show per-file compilation status
#   make help         Show this help

PROJECT_ROOT := $(dir $(realpath $(firstword $(MAKEFILE_LIST))))
BUILD_DIR    := $(PROJECT_ROOT)build
DCP_DIR      := $(PROJECT_ROOT)src/decompiled_cpp
SHIMS_DIR    := $(PROJECT_ROOT)src/sdl3_shims

# Compiler
CXX        := g++
CFLAGS     := -std=c++17 -fpermissive -g -O0
WARNFLAGS  := -Wno-error -Wno-unused -Wno-unknown-pragmas -Wno-attributes -Wno-write-strings -Wno-delete-non-virtual-dtor -Wno-narrowing
INCLUDES   := -I$(DCP_DIR) -I$(DCP_DIR)/shared -I$(DCP_DIR)/stubs -I$(DCP_DIR)/native -I$(SHIMS_DIR)
FORCE_INC  := -include $(DCP_DIR)/stubs/compat.h

# SDL3 detection
SDL3_DEV  := $(shell ls -d /nix/store/*sdl3-*-dev/include 2>/dev/null | head -1)
SDL3_LIB  := $(shell ls -d /nix/store/*sdl3-*-lib/lib 2>/dev/null | head -1)
ifneq ($(SDL3_DEV),)
  SDL3_CFLAGS  := -I$(SDL3_DEV)
else
  SDL3_CFLAGS  := $(shell pkg-config --cflags sdl3 2>/dev/null)
endif
ifneq ($(SDL3_LIB),)
  SDL3_LDFLAGS := -L$(SDL3_LIB)
else
  SDL3_LDFLAGS := $(shell pkg-config --libs-only-L sdl3 2>/dev/null)
endif
SDL3_LIBS  := -lSDL3 -lm

override CXXFLAGS := $(CFLAGS) $(WARNFLAGS) $(INCLUDES) $(FORCE_INC) $(SDL3_CFLAGS)

# Source discovery
DCP_CPP_ALL := $(filter-out $(DCP_DIR)/stubs/%, $(filter-out $(DCP_DIR)/native/%, $(wildcard $(DCP_DIR)/*/*.cpp $(DCP_DIR)/*/*/*.cpp)))

BROKEN_SRCS := $(DCP_DIR)/game/Building.cpp $(DCP_DIR)/town/Town.cpp $(DCP_DIR)/stubs/sdl3_undecompiled_stubs.cpp

DCP_CPP_SRCS := $(filter-out $(BROKEN_SRCS), $(DCP_CPP_ALL))

NATIVE_ALL := $(wildcard $(DCP_DIR)/native/*.c)
NATIVE_BROKEN := $(DCP_DIR)/native/buildingpanel_wndproc.c $(DCP_DIR)/native/config_ini.c $(DCP_DIR)/native/DDRAW_BlitHBITMAPToSurface.c $(DCP_DIR)/native/ddraw_building_sprites.c $(DCP_DIR)/native/ddraw_helpers.c $(DCP_DIR)/native/DDRAW_LoadBmpToSurface.c $(DCP_DIR)/native/game_loop_setup.c $(DCP_DIR)/native/gamestate_handlers.c $(DCP_DIR)/native/helpwnd_support.c $(DCP_DIR)/native/input_place.c $(DCP_DIR)/native/input_world.c $(DCP_DIR)/native/ui_childwindow.c $(DCP_DIR)/native/UI_DefWndProc.c $(DCP_DIR)/native/ui_manager.c $(DCP_DIR)/native/ui_position.c $(DCP_DIR)/native/UI_ProcessObjectTimers.c $(DCP_DIR)/native/ui_window_class.c $(DCP_DIR)/native/win32_network.c $(DCP_DIR)/native/win32_stream.c $(DCP_DIR)/native/winmain.c $(DCP_DIR)/native/world_enumerate_assets.c $(DCP_DIR)/native/ui_scroll_list.c $(DCP_DIR)/native/sprite_tilemap.c $(DCP_DIR)/native/math_huf_helpers.c $(DCP_DIR)/native/huf_decode.c $(DCP_DIR)/native/math_helpers.c $(DCP_DIR)/native/DDRAW_PresentRect.c $(DCP_DIR)/native/cgwnd_present.c $(DCP_DIR)/native/ui_window_update.c $(DCP_DIR)/native/win32_postquit.c $(DCP_DIR)/native/win32_thread.c
NATIVE_SRCS := $(filter-out $(NATIVE_BROKEN), $(NATIVE_ALL))

SHIM_SRCS := $(SHIMS_DIR)/resource_archive.cpp $(SHIMS_DIR)/pe_string_table.cpp $(SHIMS_DIR)/resource_manager_sdl3.cpp $(SHIMS_DIR)/sdl3_game_audio.cpp $(SHIMS_DIR)/sdl3_ddraw.cpp $(SHIMS_DIR)/sdl3_dsound.cpp $(SHIMS_DIR)/sdl3_window.cpp $(SHIMS_DIR)/sdl3_directplay_train_bridge.cpp $(SHIMS_DIR)/host_test_events.cpp $(SHIMS_DIR)/main.cpp $(SHIMS_DIR)/stub_func.c

# Derived objects
DCP_OBJS    := $(patsubst $(DCP_DIR)/%.cpp, $(BUILD_DIR)/dcp/%.o, $(DCP_CPP_SRCS))
NATIVE_OBJS := $(patsubst $(DCP_DIR)/native/%.c, $(BUILD_DIR)/native/%.o, $(NATIVE_SRCS))
SHIM_OBJS   := $(patsubst $(SHIMS_DIR)/%.cpp, $(BUILD_DIR)/shims/%.o, $(filter %.cpp, $(SHIM_SRCS)))
SHIM_OBJS   += $(patsubst $(SHIMS_DIR)/%.c, $(BUILD_DIR)/shims/%.o, $(filter %.c, $(SHIM_SRCS)))
ALL_OBJS    := $(DCP_OBJS) $(NATIVE_OBJS) $(SHIM_OBJS)
DEPFILES    := $(ALL_OBJS:.o=.d)
BINARY      := $(BUILD_DIR)/lego_loco

# ============================================================================
# Targets
# ============================================================================

.PHONY: all build run clean distclean check help dirs test test-integration test-all test-resource-archive test-resource-manager-sdl3 test-sdl3-primary-present test-mode2-menu-backdrop test-mode2-multiplayer-menu test-host-menu-renderer-linkage test-host-main-menu-accept test-host-multiplayer-menu-input test-sdl3-game-audio test-dplay-config menu-sprite-viewer run-menu-sprite-viewer test-menu-sprite-viewer

all: build

build: $(BINARY)

# Deterministic component and host-boundary suite. GUI interaction is kept in
# test-integration so agents can run the fast layer independently when needed.
test: test-resource-archive test-resource-manager-sdl3 test-dplay-config \
      test-sdl3-primary-present test-mode2-menu-backdrop \
      test-mode2-multiplayer-menu test-host-menu-renderer-linkage \
      test-host-main-menu-accept test-host-multiplayer-menu-input \
      test-sdl3-game-audio test-menu-sprite-viewer

test-integration: $(BINARY)
	@python3 -m pytest -v -m "integration and gui" tests/integration

test-all: test test-integration

# Link
$(BINARY): $(ALL_OBJS) | dirs
	$(CXX) -std=c++17 $(ALL_OBJS) $(SDL3_LDFLAGS) $(SDL3_LIBS) -Wl,--allow-multiple-definition -Wl,--unresolved-symbols=ignore-all -o $@

# Compilation rules
$(BUILD_DIR)/dcp/%.o: $(DCP_DIR)/%.cpp | dirs
	$(CXX) $(CXXFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(BUILD_DIR)/native/%.o: $(DCP_DIR)/native/%.c | dirs
	$(CXX) $(CXXFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(BUILD_DIR)/shims/%.o: $(SHIMS_DIR)/%.cpp | dirs
	$(CXX) $(CXXFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(BUILD_DIR)/shims/%.o: $(SHIMS_DIR)/%.c | dirs
	$(CXX) $(CXXFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

# Rebuild all objects if build rules change; otherwise generated .d files
# rebuild every consumer of a changed header (critical for C++ vtables).
$(ALL_OBJS): Makefile
-include $(DEPFILES)

# Dirs
dirs:
	@mkdir -p $(sort $(dir $(ALL_OBJS)))

# Asset archive regression test — validates real RFH indexing and Huf_Decode @ 0x45C830.
RESOURCE_ARCHIVE_TEST := $(BUILD_DIR)/resource_archive_test

$(RESOURCE_ARCHIVE_TEST): $(SHIMS_DIR)/resource_archive.cpp $(SHIMS_DIR)/resource_archive.h $(SHIMS_DIR)/pe_string_table.cpp $(SHIMS_DIR)/pe_string_table.h tests/resource_archive_test.cpp | dirs
	@echo "=== Testing RFD/RFH archive ==="
	@$(CXX) -std=c++17 -Wall -Wextra -Werror $(SHIMS_DIR)/resource_archive.cpp $(SHIMS_DIR)/pe_string_table.cpp tests/resource_archive_test.cpp -o $@

test-resource-archive: $(RESOURCE_ARCHIVE_TEST)
	@$(RESOURCE_ARCHIVE_TEST) $(PROJECT_ROOT)lego-loco-unpacked/art-res

RESOURCE_MANAGER_SDL3_TEST := $(BUILD_DIR)/resource_manager_sdl3_test

$(RESOURCE_MANAGER_SDL3_TEST): $(SHIMS_DIR)/resource_archive.cpp $(SHIMS_DIR)/resource_archive.h $(SHIMS_DIR)/pe_string_table.cpp $(SHIMS_DIR)/pe_string_table.h $(SHIMS_DIR)/resource_manager_sdl3.cpp $(SHIMS_DIR)/resource_manager_sdl3.h tests/resource_manager_sdl3_test.cpp | dirs
	@echo "=== Testing ResourceManager sprite bridge ==="
	@$(CXX) -std=c++17 -Wall -Wextra -Werror $(SDL3_CFLAGS) $(SHIMS_DIR)/resource_archive.cpp $(SHIMS_DIR)/pe_string_table.cpp $(SHIMS_DIR)/resource_manager_sdl3.cpp tests/resource_manager_sdl3_test.cpp $(SDL3_LDFLAGS) $(SDL3_LIBS) -o $@

test-resource-manager-sdl3: $(RESOURCE_MANAGER_SDL3_TEST)
	@SDL3_LIB="$(SDL3_LIB)"; if [ -n "$$SDL3_LIB" ]; then export LD_LIBRARY_PATH="$$SDL3_LIB:$$LD_LIBRARY_PATH"; fi; cd $(PROJECT_ROOT) && SDL_VIDEODRIVER=dummy $(RESOURCE_MANAGER_SDL3_TEST)

# DPlayConfig regression: defaults recovered from GameConfig_constructor @ 0x440C60.
DPLAY_CONFIG_TEST := $(BUILD_DIR)/dplay_config_test

$(DPLAY_CONFIG_TEST): $(DCP_DIR)/network/DPlayConfig.h tests/dplay_config_test.cpp | dirs
	@echo "=== Testing DPlayConfig defaults ==="
	@$(CXX) -std=c++17 -Wall -Wextra -Werror tests/dplay_config_test.cpp -o $@

test-dplay-config: $(DPLAY_CONFIG_TEST)
	@$(DPLAY_CONFIG_TEST)

# SDL primary-target regression: validates the CGWND frame source reaches the window.
SDL3_PRIMARY_PRESENT_TEST := $(BUILD_DIR)/sdl3_primary_present_test

$(SDL3_PRIMARY_PRESENT_TEST): $(SHIMS_DIR)/sdl3_window.cpp $(SHIMS_DIR)/sdl3_window.h $(SHIMS_DIR)/sdl3_ddraw.cpp $(SHIMS_DIR)/sdl3_ddraw.h tests/sdl3_primary_present_test.cpp | dirs
	@echo "=== Testing SDL primary-surface presentation ==="
	@$(CXX) -std=c++17 -Wall -Wextra -Werror $(FORCE_INC) $(SDL3_CFLAGS) $(SHIMS_DIR)/sdl3_window.cpp $(SHIMS_DIR)/sdl3_ddraw.cpp tests/sdl3_primary_present_test.cpp $(SDL3_LDFLAGS) $(SDL3_LIBS) -o $@

test-sdl3-primary-present: $(SDL3_PRIMARY_PRESENT_TEST)
	@SDL3_LIB="$(SDL3_LIB)"; if [ -n "$$SDL3_LIB" ]; then export LD_LIBRARY_PATH="$$SDL3_LIB:$$LD_LIBRARY_PATH"; fi; SDL_VIDEODRIVER=dummy $(SDL3_PRIMARY_PRESENT_TEST)

# Mode-2 EditWindow::render regression: recovered backdrop resources reach the SDL primary target.
MODE2_MENU_BACKDROP_TEST := $(BUILD_DIR)/mode2_menu_backdrop_test

$(MODE2_MENU_BACKDROP_TEST): $(SHIMS_DIR)/resource_archive.cpp $(SHIMS_DIR)/resource_archive.h $(SHIMS_DIR)/pe_string_table.cpp $(SHIMS_DIR)/pe_string_table.h $(SHIMS_DIR)/resource_manager_sdl3.cpp $(SHIMS_DIR)/resource_manager_sdl3.h $(SHIMS_DIR)/sdl3_window.cpp $(SHIMS_DIR)/sdl3_window.h $(SHIMS_DIR)/sdl3_ddraw.cpp $(SHIMS_DIR)/sdl3_ddraw.h tests/mode2_menu_backdrop_test.cpp | dirs
	@echo "=== Testing mode 2 EditWindow backdrop composition ==="
	@$(CXX) -std=c++17 -Wall -Wextra -Werror $(FORCE_INC) $(SDL3_CFLAGS) $(SHIMS_DIR)/resource_archive.cpp $(SHIMS_DIR)/pe_string_table.cpp $(SHIMS_DIR)/resource_manager_sdl3.cpp $(SHIMS_DIR)/sdl3_window.cpp $(SHIMS_DIR)/sdl3_ddraw.cpp tests/mode2_menu_backdrop_test.cpp $(SDL3_LDFLAGS) $(SDL3_LIBS) -o $@

test-mode2-menu-backdrop: $(MODE2_MENU_BACKDROP_TEST)
	@SDL3_LIB="$(SDL3_LIB)"; if [ -n "$$SDL3_LIB" ]; then export LD_LIBRARY_PATH="$$SDL3_LIB:$$LD_LIBRARY_PATH"; fi; cd $(PROJECT_ROOT) && SDL_VIDEODRIVER=dummy $(MODE2_MENU_BACKDROP_TEST)

# Mode-2 multiplayer GameSetupPanel host-composition regression.
MODE2_MULTIPLAYER_MENU_TEST := $(BUILD_DIR)/mode2_multiplayer_menu_test

$(MODE2_MULTIPLAYER_MENU_TEST): $(SHIMS_DIR)/resource_archive.cpp $(SHIMS_DIR)/resource_archive.h $(SHIMS_DIR)/pe_string_table.cpp $(SHIMS_DIR)/pe_string_table.h $(SHIMS_DIR)/resource_manager_sdl3.cpp $(SHIMS_DIR)/resource_manager_sdl3.h $(SHIMS_DIR)/sdl3_window.cpp $(SHIMS_DIR)/sdl3_window.h $(SHIMS_DIR)/sdl3_ddraw.cpp $(SHIMS_DIR)/sdl3_ddraw.h tests/mode2_multiplayer_menu_test.cpp | dirs
	@echo "=== Testing mode 2 multiplayer menu composition ==="
	@$(CXX) -std=c++17 -Wall -Wextra -Werror $(FORCE_INC) $(SDL3_CFLAGS) $(SHIMS_DIR)/resource_archive.cpp $(SHIMS_DIR)/pe_string_table.cpp $(SHIMS_DIR)/resource_manager_sdl3.cpp $(SHIMS_DIR)/sdl3_window.cpp $(SHIMS_DIR)/sdl3_ddraw.cpp tests/mode2_multiplayer_menu_test.cpp $(SDL3_LDFLAGS) $(SDL3_LIBS) -o $@

test-mode2-multiplayer-menu: $(MODE2_MULTIPLAYER_MENU_TEST)
	@SDL3_LIB="$(SDL3_LIB)"; if [ -n "$$SDL3_LIB" ]; then export LD_LIBRARY_PATH="$$SDL3_LIB:$$LD_LIBRARY_PATH"; fi; cd $(PROJECT_ROOT) && SDL_VIDEODRIVER=dummy $(MODE2_MULTIPLAYER_MENU_TEST)

# Regression for the C-linkage renderer lookup used after main-menu Enter.
test-host-menu-renderer-linkage: $(BINARY) tests/host_menu_renderer_linkage_test.sh
	@tests/host_menu_renderer_linkage_test.sh $(BINARY)

# Regression for host-only routing of SDL clicks into GameSetupPanel's
# GAMESTATE_HandleClick-derived control adapter.
# Regression for the resource-0x403 accept click, which directly calls
# EditWindow_OnPlayerNameChanged at original address 0x422AB2.
test-host-main-menu-accept: tests/host_main_menu_accept_test.sh
	@tests/host_main_menu_accept_test.sh

# Regression for host-only routing of SDL clicks into GameSetupPanel control adapter.
test-host-multiplayer-menu-input: tests/host_multiplayer_menu_input_test.sh
	@tests/host_multiplayer_menu_input_test.sh

# Host sound regression: mode 2 preloads 0x5015; mode 10 plays 0x5026.
SDL3_GAME_AUDIO_TEST := $(BUILD_DIR)/sdl3_game_audio_test

$(SDL3_GAME_AUDIO_TEST): $(SHIMS_DIR)/resource_archive.cpp $(SHIMS_DIR)/resource_archive.h $(SHIMS_DIR)/pe_string_table.cpp $(SHIMS_DIR)/pe_string_table.h $(SHIMS_DIR)/resource_manager_sdl3.cpp $(SHIMS_DIR)/resource_manager_sdl3.h $(SHIMS_DIR)/sdl3_game_audio.cpp $(SHIMS_DIR)/sdl3_game_audio.h tests/sdl3_game_audio_test.cpp | dirs
	@echo "=== Testing host game audio ==="
	@$(CXX) -std=c++17 -Wall -Wextra -Werror $(SDL3_CFLAGS) $(SHIMS_DIR)/resource_archive.cpp $(SHIMS_DIR)/pe_string_table.cpp $(SHIMS_DIR)/resource_manager_sdl3.cpp $(SHIMS_DIR)/sdl3_game_audio.cpp tests/sdl3_game_audio_test.cpp $(SDL3_LDFLAGS) $(SDL3_LIBS) -o $@

test-sdl3-game-audio: $(SDL3_GAME_AUDIO_TEST)
	@SDL3_LIB="$(SDL3_LIB)"; if [ -n "$$SDL3_LIB" ]; then export LD_LIBRARY_PATH="$$SDL3_LIB:$$LD_LIBRARY_PATH"; fi; cd $(PROJECT_ROOT) && SDL_AUDIODRIVER=dummy $(SDL3_GAME_AUDIO_TEST)

# Startup menu sprite diagnostic: real PE IDs → RFH paths → RFD BMPs → SDL textures.
MENU_SPRITE_VIEWER := $(BUILD_DIR)/menu_sprite_viewer

$(MENU_SPRITE_VIEWER): $(SHIMS_DIR)/resource_archive.cpp $(SHIMS_DIR)/resource_archive.h $(SHIMS_DIR)/pe_string_table.cpp $(SHIMS_DIR)/pe_string_table.h $(SHIMS_DIR)/resource_manager_sdl3.cpp $(SHIMS_DIR)/resource_manager_sdl3.h tools/menu_sprite_viewer.cpp | dirs
	@echo "=== Building real startup sprite viewer ==="
	@$(CXX) -std=c++17 -Wall -Wextra -Werror $(SDL3_CFLAGS) $(SHIMS_DIR)/resource_archive.cpp $(SHIMS_DIR)/pe_string_table.cpp $(SHIMS_DIR)/resource_manager_sdl3.cpp tools/menu_sprite_viewer.cpp $(SDL3_LDFLAGS) $(SDL3_LIBS) -o $@

menu-sprite-viewer: $(MENU_SPRITE_VIEWER)

run-menu-sprite-viewer: $(MENU_SPRITE_VIEWER)
	@SDL3_LIB="$(SDL3_LIB)"; if [ -n "$$SDL3_LIB" ]; then export LD_LIBRARY_PATH="$$SDL3_LIB:$$LD_LIBRARY_PATH"; fi; cd $(PROJECT_ROOT) && exec $(MENU_SPRITE_VIEWER)

test-menu-sprite-viewer: $(MENU_SPRITE_VIEWER)
	@echo "=== Rendering one real startup-menu frame (SDL dummy driver) ==="
	@SDL3_LIB="$(SDL3_LIB)"; if [ -n "$$SDL3_LIB" ]; then export LD_LIBRARY_PATH="$$SDL3_LIB:$$LD_LIBRARY_PATH"; fi; cd $(PROJECT_ROOT) && SDL_VIDEODRIVER=dummy $(MENU_SPRITE_VIEWER) --frames 1

# Run
run: $(BINARY)
	@echo "=== Running Lego Loco SDL3 ==="
	@SDL3_LIB="$(SDL3_LIB)"; if [ -n "$$SDL3_LIB" ]; then export LD_LIBRARY_PATH="$$SDL3_LIB:$$LD_LIBRARY_PATH"; fi; if [ -d "$(PROJECT_ROOT)lego-loco-unpacked" ]; then export LEGO_LOCO_DATA="$(PROJECT_ROOT)lego-loco-unpacked"; fi; cd $(PROJECT_ROOT) && exec $(BINARY)

# Clean
clean:
	@echo "=== Cleaning ==="
	@rm -rf $(BUILD_DIR)/dcp $(BUILD_DIR)/native $(BUILD_DIR)/shims
	@rm -f $(BINARY) $(BUILD_DIR)/link.err
	@echo "  Done."

distclean: clean
	@rm -f $(BINARY) $(BUILD_DIR)/link.err
	@echo "  distclean done."

# Status
check:
	@echo "=== Lego Loco Build Status ==="
	@echo ""
	@echo "Compiler:  $(CXX)"
	@echo "SDL3 dev:  $(SDL3_DEV)"
	@echo "SDL3 lib:  $(SDL3_LIB)"
	@echo ""
	@echo "--- Sources ---"
	@echo "  C++:      $(words $(DCP_CPP_SRCS)) (+$(words $(BROKEN_SRCS)) broken)"
	@echo "  Native C: $(words $(NATIVE_SRCS)) (+$(words $(NATIVE_BROKEN)) broken)"
	@echo "  Shims:    $(words $(SHIM_SRCS))"
	@echo "  Total objs: $(words $(ALL_OBJS))"
	@echo ""
	@echo "--- Broken (skipped) ---"
	@for f in $(BROKEN_SRCS); do echo "  [SKIP] $$(echo $$f | sed 's|$(DCP_DIR)/||')"; done
	@echo ""
	@built=$$(find $(BUILD_DIR) -name '*.o' 2>/dev/null | wc -l); echo "  Objects built: $$built / $(words $(ALL_OBJS))"; if [ -f $(BINARY) ]; then echo "  Binary: $(BINARY) ($$(ls -lh $(BINARY) | awk '{print $$5}'))"; else echo "  Binary: not built"; fi

# Help
help:
	@echo "Lego Loco SDL3 -- Unified Build"
	@echo ""
	@echo "  make          Build everything"
	@echo "  make run              Build and run"
	@echo "  make test             Run deterministic regressions"
	@echo "  make test-integration Run isolated Wayland GUI tests"
	@echo "  make test-all         Run every test layer"
	@echo "  make clean            Remove generated build outputs"
	@echo "  make distclean Reset everything"
	@echo "  make check    Show status"


_test:
	@echo "PROJECT_ROOT=[$(PROJECT_ROOT)]"
	@echo "BUILD_DIR=[$(BUILD_DIR)]"
	@echo "SHIMS_DIR=[$(SHIMS_DIR)]"
	@echo "SHIM_SRCS=[$(SHIM_SRCS)]"
	@echo "SHIM_OBJS=[$(SHIM_OBJS)]"
