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
# Anti-pattern compiler flags (see AGENTS.md "Fix anti-patterns on sight")
# Three tiers:
#   make                    Tier 1 — always-on ABI/layout correctness checks
#   make STRICT=1           Tier 1+2 — calling-convention and cv/null checks
#   make STRICT=2           Tier 1+3 — old-style-cast and Effective C++ audits
#
# Tier 1 flags catch: class-memaccess (raw writes on vtable-bearing types),
# cast-function-type (vtable dispatch casts), strict-aliasing (type-punned
# offset access), non-virtual-dtor (missing virtual destructors), reorder
# (initializer-order mismatches), return-type (silent stub returns),
# overloaded-virtual (hidden virtuals), suggest-override (missing override),
# narrowing, write-strings, int-to-pointer-cast, pointer-arith.
WARNFLAGS  := -Werror=delete-non-virtual-dtor \
              -Werror=non-virtual-dtor \
              -Werror=class-memaccess \
              -Werror=cast-function-type \
              -Werror=return-type \
              -Werror=strict-aliasing \
              -Werror=write-strings \
              -Werror=narrowing \
              -Werror=reorder \
              -Werror=overloaded-virtual \
              -Werror=pmf-conversions \
              -Werror=invalid-offsetof \
              -Werror=missing-field-initializers \
              -Werror=suggest-override \
              -Werror=subobject-linkage \
              -Werror=cast-align \
              -Werror=int-to-pointer-cast \
              -Werror=pointer-arith \
              -Wno-unknown-pragmas \
              -Wno-cpp \
              -Wno-unused \
              -Wno-unused-parameter \
              -Wno-attributes
# Tier 2 — STRICT=1: diagnose ignored attributes and tighten type hygiene.
# GCC reports an ignored calling-convention attribute in -Wattributes;
# -Werror=ignored-attributes does not promote that diagnostic.
ifdef STRICT
  ifeq ($(STRICT),1)
    WARNFLAGS += -Wattributes -Werror=attributes -Werror=cast-qual -Werror=zero-as-null-pointer-constant
  else ifeq ($(STRICT),2)
    WARNFLAGS += -Wattributes -Werror=attributes -Werror=old-style-cast -Werror=cast-qual -Werror=zero-as-null-pointer-constant
    WARNFLAGS += -Weffc++ -Werror=missing-declarations
  endif
endif
INCLUDES   := -I/nix/store/qmajm5vxiwziaw4d34d5mwiy39d65wj4-freetype-2.14.3-dev/include -I$(DCP_DIR) -I$(DCP_DIR)/shared -I$(DCP_DIR)/stubs -I$(DCP_DIR)/native -I$(SHIMS_DIR)
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
# SDL_net 3 provides the host-only TCP listener/client transport.
SDL3_NET_DEV := $(shell ls -d /nix/store/*-sdl3-net-*-dev/include 2>/dev/null | head -1)
SDL3_NET_LIB := $(shell ls -d /nix/store/*-sdl3-net-*-lib/lib 2>/dev/null | head -1)
ifneq ($(SDL3_NET_DEV),)
  SDL3_NET_CFLAGS := -I$(SDL3_NET_DEV)
else
  SDL3_NET_CFLAGS := $(shell pkg-config --cflags sdl3-net 2>/dev/null)
endif
ifneq ($(SDL3_NET_LIB),)
  SDL3_NET_LDFLAGS := -L$(SDL3_NET_LIB)
else
  SDL3_NET_LDFLAGS := $(shell pkg-config --libs-only-L sdl3-net 2>/dev/null)
endif
SDL3_NET_LIBS := -lSDL3_net
# The SDL-hosted intro player uses GStreamer appsink to decode Cinepak AVI
# frames into the SDL renderer. Its headers/libs are provided by flake.nix.
GST_CFLAGS := $(shell pkg-config --cflags gstreamer-app-1.0 gstreamer-video-1.0)
GST_LIBS   := $(shell pkg-config --libs gstreamer-app-1.0 gstreamer-video-1.0)
# GameSetupPanel list text follows ResourceManager_Init (0x44611A): 14px Arial,
# weight 700. Fontconfig supplies the host-compatible face; FreeType rasterizes it.
FREETYPE_CFLAGS := $(shell pkg-config --cflags freetype2)
FREETYPE_LIBS   := $(shell pkg-config --libs freetype2)
# Desktop Linux discovers DNS-SD sessions through Avahi on the system D-Bus.
DBUS_CFLAGS := $(shell pkg-config --cflags dbus-1 2>/dev/null)
DBUS_LIBS   := $(shell pkg-config --libs dbus-1 2>/dev/null)
HOST_UI_FONT_FILE := $(shell fc-match -f '%{file}' Arial 2>/dev/null | head -n 1)

override CXXFLAGS := $(CFLAGS) $(WARNFLAGS) -pthread $(INCLUDES) $(FORCE_INC) $(SDL3_CFLAGS) $(SDL3_NET_CFLAGS) $(GST_CFLAGS) $(FREETYPE_CFLAGS) $(DBUS_CFLAGS) -DLOCO_HOST_UI_FONT_FILE=\"$(HOST_UI_FONT_FILE)\"

# Source discovery
DCP_CPP_ALL := $(filter-out $(DCP_DIR)/stubs/%, $(filter-out $(DCP_DIR)/native/%, $(wildcard $(DCP_DIR)/*/*.cpp $(DCP_DIR)/*/*/*.cpp)))

BROKEN_SRCS :=

DCP_CPP_SRCS := $(filter-out $(BROKEN_SRCS), $(DCP_CPP_ALL))

NATIVE_ALL := $(wildcard $(DCP_DIR)/native/*.c)
NATIVE_BROKEN := $(DCP_DIR)/native/buildingpanel_wndproc.c $(DCP_DIR)/native/config_ini.c $(DCP_DIR)/native/DDRAW_BlitHBITMAPToSurface.c $(DCP_DIR)/native/ddraw_building_sprites.c $(DCP_DIR)/native/ddraw_helpers.c $(DCP_DIR)/native/DDRAW_LoadBmpToSurface.c $(DCP_DIR)/native/game_loop_setup.c $(DCP_DIR)/native/gamestate_handlers.c $(DCP_DIR)/native/helpwnd_support.c $(DCP_DIR)/native/input_place.c $(DCP_DIR)/native/input_world.c $(DCP_DIR)/native/input_manager.c $(DCP_DIR)/native/ui_childwindow.c $(DCP_DIR)/native/UI_DefWndProc.c $(DCP_DIR)/native/ui_manager.c $(DCP_DIR)/native/ui_position.c $(DCP_DIR)/native/UI_ProcessObjectTimers.c $(DCP_DIR)/native/ui_window_class.c $(DCP_DIR)/native/win32_network.c $(DCP_DIR)/native/win32_stream.c $(DCP_DIR)/native/winmain.c $(DCP_DIR)/native/world_enumerate_assets.c $(DCP_DIR)/native/ui_scroll_list.c $(DCP_DIR)/native/sprite_tilemap.c $(DCP_DIR)/native/math_huf_helpers.c $(DCP_DIR)/native/huf_decode.c $(DCP_DIR)/native/math_helpers.c $(DCP_DIR)/native/DDRAW_PresentRect.c $(DCP_DIR)/native/cgwnd_present.c $(DCP_DIR)/native/ui_window_update.c $(DCP_DIR)/native/win32_postquit.c $(DCP_DIR)/native/win32_thread.c $(DCP_DIR)/native/stream_lock.c
NATIVE_SRCS := $(filter-out $(NATIVE_BROKEN), $(NATIVE_ALL))

SHIM_SRCS := $(SHIMS_DIR)/sdl3_town_mode3.cpp $(SHIMS_DIR)/sdl3_ddraw.cpp $(SHIMS_DIR)/sdl3_dsound.cpp $(SHIMS_DIR)/sdl3_window.cpp $(SHIMS_DIR)/sdl3_game_audio.cpp $(SHIMS_DIR)/sdl3_net_stubs.cpp $(SHIMS_DIR)/resource_archive.cpp $(SHIMS_DIR)/pe_string_table.cpp $(SHIMS_DIR)/resource_manager_sdl3.cpp $(SHIMS_DIR)/sdl3_intro_video.cpp $(SHIMS_DIR)/main.cpp $(SHIMS_DIR)/stub_func.c $(SHIMS_DIR)/host_test_events.cpp

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

.PHONY: all build run clean distclean check help dirs diagnostic-census test test-integration test-all test-sdl3-net-protocol test-sdl3-net-transport test-sdl3-net-runtime test-sdl3-net-discovery-transport test-network-discovery test-discovery-runtime test-avahi-dbus-discovery test-embedded-mdns-discovery test-resource-archive test-resource-manager-sdl3 test-sdl3-primary-present test-mode2-menu-backdrop test-mode2-multiplayer-menu test-host-menu-renderer-linkage test-host-main-menu-accept test-host-multiplayer-selector test-host-multiplayer-menu-input test-sdl3-game-audio test-dplay-config test-postbag-cleanup test-cgwnd-entermode3 test-host-mode3-bootstrap test-inputmgr-canonical test-persistence-adapter test-input-world test-intro-video-sequence test-host-intro-video-linkage test-sdl3-timer-stress test-uipanel-surface-linkage menu-sprite-viewer run-menu-sprite-viewer test-menu-sprite-viewer

all: build

build: $(BINARY)

test: test-unit

# Deterministic component and host-boundary suite. GUI integration is kept in
# test-integration. Use `make test-all` to run both layers.
test-unit: test-sdl3-net-protocol test-sdl3-net-transport \
      test-sdl3-net-runtime test-sdl3-net-discovery-transport \
      test-network-discovery test-discovery-runtime \
      test-avahi-dbus-discovery test-embedded-mdns-discovery \
      test-resource-archive test-resource-manager-sdl3 \
      test-dplay-config test-postbag-cleanup \
      test-cgwnd-entermode3 test-host-mode3-bootstrap test-inputmgr-canonical \
      test-persistence-adapter test-input-world \
      test-sdl3-primary-present test-mode2-menu-backdrop \
      test-mode2-multiplayer-menu test-host-menu-renderer-linkage \
      test-host-main-menu-accept test-host-multiplayer-selector \
      test-host-multiplayer-menu-input test-sdl3-game-audio test-intro-video-sequence \
      test-host-intro-video-linkage test-sdl3-timer-stress test-uipanel-surface-linkage \
      test-menu-sprite-viewer

# SDL3 timer safety regression: rapid SetTimer/KillTimer cycles under thread contention.
SDL3_TIMER_STRESS_TEST := $(BUILD_DIR)/sdl3_timer_stress_test

$(SDL3_TIMER_STRESS_TEST): $(SHIMS_DIR)/sdl3_window.cpp $(SHIMS_DIR)/sdl3_window.h $(SHIMS_DIR)/sdl3_game_audio.cpp $(SHIMS_DIR)/sdl3_game_audio.h $(SHIMS_DIR)/resource_manager_sdl3.cpp $(SHIMS_DIR)/resource_manager_sdl3.h $(SHIMS_DIR)/resource_archive.cpp $(SHIMS_DIR)/resource_archive.h $(SHIMS_DIR)/pe_string_table.cpp $(SHIMS_DIR)/pe_string_table.h $(SHIMS_DIR)/host_test_events.cpp $(SHIMS_DIR)/host_test_events.h tests/sdl3_timer_stress_test.cpp | dirs
	@echo "=== Testing SDL3 timer safety (use-after-free regression) ==="
	@$(CXX) -std=c++17 -Wall -Wextra -Werror $(FORCE_INC) $(SDL3_CFLAGS) $(SHIMS_DIR)/sdl3_window.cpp $(SHIMS_DIR)/sdl3_game_audio.cpp $(SHIMS_DIR)/resource_manager_sdl3.cpp $(SHIMS_DIR)/resource_archive.cpp $(SHIMS_DIR)/pe_string_table.cpp $(SHIMS_DIR)/host_test_events.cpp tests/sdl3_timer_stress_test.cpp $(SDL3_LDFLAGS) $(SDL3_LIBS) -o $@

test-sdl3-timer-stress: $(SDL3_TIMER_STRESS_TEST)
	@SDL3_LIB="$(SDL3_LIB)"; if [ -n "$$SDL3_LIB" ]; then export LD_LIBRARY_PATH="$$SDL3_LIB:$$LD_LIBRARY_PATH"; fi; SDL_VIDEODRIVER=dummy $(SDL3_TIMER_STRESS_TEST)

test-uipanel-surface-linkage: $(BINARY) tests/uipanel_surface_linkage_test.sh
	@bash tests/uipanel_surface_linkage_test.sh $(BINARY)

test-integration: $(BINARY) $(BUILD_DIR)/sdl3_net_transport_test
	@python3 -m pytest -v -m "integration and gui" tests/integration

test-all: test test-integration

# Reproducible full-build diagnostic census. A temporary BUILD_DIR keeps the
# clean/-k run and its logs out of the repository's generated build tree.
diagnostic-census:
	@set -eu; \
	 tmp="$$(mktemp -d "$${TMPDIR:-/tmp}/lego-loco-root-census.XXXXXX")"; \
	 trap 'rm -rf "$$tmp"' EXIT; \
	 echo "=== Full root diagnostic census (temporary build: $$tmp) ==="; \
	 $(MAKE) --no-print-directory BUILD_DIR="$$tmp/build" clean >/dev/null; \
	 status=0; $(MAKE) --no-print-directory -k BUILD_DIR="$$tmp/build" all >"$$tmp/build.log" 2>&1 || status=$$?; \
	 errors="$$(grep -Ec 'error:' "$$tmp/build.log" || true)"; \
	 warnings="$$(grep -Ec 'warning:' "$$tmp/build.log" || true)"; \
	 failed="$$(grep -Ec 'Error [0-9]+' "$$tmp/build.log" || true)"; \
	 objects="$$(find "$$tmp/build" -name '*.o' -type f 2>/dev/null | wc -l)"; \
	 printf 'status=%s errors=%s warnings=%s failed-recipes=%s objects=%s\n' "$$status" "$$errors" "$$warnings" "$$failed" "$$objects"; \
	 cat "$$tmp/build.log"; \
	 exit "$$status"

# Transport codec and real two-process SDL_net loopback regressions.
SDL3_NET_PROTOCOL_TEST := $(BUILD_DIR)/sdl3_net_protocol_test
SDL3_NET_TRANSPORT_TEST := $(BUILD_DIR)/sdl3_net_transport_test

$(SDL3_NET_PROTOCOL_TEST): $(SHIMS_DIR)/sdl3_net_protocol.cpp $(SHIMS_DIR)/sdl3_net_protocol.h tests/sdl3_net_protocol_test.cpp | dirs
	@echo "=== Testing SDL_net transport protocol ==="
	@$(CXX) -std=c++17 -Wall -Wextra -Werror -I$(SHIMS_DIR) $(SHIMS_DIR)/sdl3_net_protocol.cpp tests/sdl3_net_protocol_test.cpp -o $@

test-sdl3-net-protocol: $(SDL3_NET_PROTOCOL_TEST)
	@$(SDL3_NET_PROTOCOL_TEST)

$(SDL3_NET_TRANSPORT_TEST): $(SHIMS_DIR)/sdl3_net_protocol.cpp $(SHIMS_DIR)/sdl3_net_protocol.h $(SHIMS_DIR)/sdl3_net_transport.cpp $(SHIMS_DIR)/sdl3_net_transport.h tests/sdl3_net_transport_test.cpp | dirs
	@echo "=== Building SDL_net two-process loopback transport test ==="
	@$(CXX) -std=c++17 -Wall -Wextra -Werror -pthread -I$(SHIMS_DIR) $(SDL3_CFLAGS) $(SDL3_NET_CFLAGS) $(SHIMS_DIR)/sdl3_net_protocol.cpp $(SHIMS_DIR)/sdl3_net_transport.cpp tests/sdl3_net_transport_test.cpp $(SDL3_NET_LDFLAGS) $(SDL3_NET_LIBS) $(SDL3_LDFLAGS) $(SDL3_LIBS) -o $@

test-sdl3-net-transport: $(SDL3_NET_TRANSPORT_TEST)
	@SDL3_LIB="$(SDL3_LIB)"; SDL3_NET_LIB="$(SDL3_NET_LIB)"; export LD_LIBRARY_PATH="$$SDL3_NET_LIB:$$SDL3_LIB:$$LD_LIBRARY_PATH"; tests/sdl3_net_transport_test.sh $(SDL3_NET_TRANSPORT_TEST)

SDL3_NET_RUNTIME_TEST := $(BUILD_DIR)/sdl3_net_runtime_test

$(SDL3_NET_RUNTIME_TEST): $(SHIMS_DIR)/sdl3_net_protocol.cpp $(SHIMS_DIR)/sdl3_net_transport.cpp $(SHIMS_DIR)/sdl3_net_runtime.cpp tests/sdl3_net_runtime_test.cpp | dirs
	@echo "=== Testing dedicated SDL_net transport worker ==="
	@$(CXX) -std=c++17 -Wall -Wextra -Werror -pthread -I$(SHIMS_DIR) $(SDL3_CFLAGS) $(SDL3_NET_CFLAGS) $(SHIMS_DIR)/sdl3_net_protocol.cpp $(SHIMS_DIR)/sdl3_net_transport.cpp $(SHIMS_DIR)/sdl3_net_runtime.cpp tests/sdl3_net_runtime_test.cpp $(SDL3_NET_LDFLAGS) $(SDL3_NET_LIBS) $(SDL3_LDFLAGS) $(SDL3_LIBS) -o $@

test-sdl3-net-runtime: $(SDL3_NET_RUNTIME_TEST)
	@SDL3_LIB="$(SDL3_LIB)"; SDL3_NET_LIB="$(SDL3_NET_LIB)"; export LD_LIBRARY_PATH="$$SDL3_NET_LIB:$$SDL3_LIB:$$LD_LIBRARY_PATH"; $(SDL3_NET_RUNTIME_TEST)

# Discovery abstraction regression: platform-neutral backend selection and failover.
NETWORK_DISCOVERY_TEST := $(BUILD_DIR)/network_discovery_test

$(NETWORK_DISCOVERY_TEST): $(SHIMS_DIR)/network_discovery.cpp $(SHIMS_DIR)/network_discovery.h tests/network_discovery_test.cpp | dirs
	@echo "=== Testing discovery backend abstraction ==="
	@$(CXX) -std=c++17 -Wall -Wextra -Werror -I$(SHIMS_DIR) $(SHIMS_DIR)/network_discovery.cpp tests/network_discovery_test.cpp -o $@

test-network-discovery: $(NETWORK_DISCOVERY_TEST)
	@$(NETWORK_DISCOVERY_TEST)

DISCOVERY_RUNTIME_TEST := $(BUILD_DIR)/discovery_runtime_test

$(DISCOVERY_RUNTIME_TEST): $(SHIMS_DIR)/discovery_runtime.cpp $(SHIMS_DIR)/discovery_runtime.h $(SHIMS_DIR)/network_discovery.cpp $(SHIMS_DIR)/network_discovery.h tests/discovery_runtime_test.cpp | dirs
	@echo "=== Testing discovery worker runtime ==="
	@$(CXX) -std=c++17 -Wall -Wextra -Werror -pthread -I$(SHIMS_DIR) $(SHIMS_DIR)/network_discovery.cpp $(SHIMS_DIR)/discovery_runtime.cpp tests/discovery_runtime_test.cpp -o $@

test-discovery-runtime: $(DISCOVERY_RUNTIME_TEST)
	@$(DISCOVERY_RUNTIME_TEST)

# Avahi adapter regression runs against an isolated fake system service on a
# private dbus-daemon; it never modifies or depends on the host Avahi daemon.
AVAHI_DBUS_DISCOVERY_TEST := $(BUILD_DIR)/avahi_dbus_discovery_test

$(AVAHI_DBUS_DISCOVERY_TEST): $(SHIMS_DIR)/avahi_dbus_discovery.cpp $(SHIMS_DIR)/avahi_dbus_discovery.h $(SHIMS_DIR)/network_discovery_protocol.cpp $(SHIMS_DIR)/network_discovery_protocol.h $(SHIMS_DIR)/network_discovery.h tests/avahi_dbus_discovery_test.cpp | dirs
	@echo "=== Testing Avahi D-Bus discovery backend ==="
	@$(CXX) -std=c++17 -Wall -Wextra -Werror -pthread -I$(SHIMS_DIR) $(DBUS_CFLAGS) $(SHIMS_DIR)/network_discovery_protocol.cpp $(SHIMS_DIR)/avahi_dbus_discovery.cpp tests/avahi_dbus_discovery_test.cpp $(DBUS_LIBS) -o $@

test-avahi-dbus-discovery: $(AVAHI_DBUS_DISCOVERY_TEST)
	@set -eu; info="$$(dbus-daemon --session --fork --print-address=1 --print-pid=1)"; \
	  address="$$(printf '%s\n' "$$info" | sed -n '1p')"; \
	  pid="$$(printf '%s\n' "$$info" | sed -n '2p')"; \
	  trap 'kill "$$pid" 2>/dev/null || true' EXIT; \
	  DBUS_SYSTEM_BUS_ADDRESS="$$address" $(AVAHI_DBUS_DISCOVERY_TEST)

# Run the embedded responder with sole control of UDP 5353 in a private
# user/network namespace; host Avahi/systemd-resolved remains untouched.
EMBEDDED_MDNS_DISCOVERY_TEST := $(BUILD_DIR)/embedded_mdns_discovery_test

$(EMBEDDED_MDNS_DISCOVERY_TEST): $(SHIMS_DIR)/embedded_mdns_discovery.cpp $(SHIMS_DIR)/embedded_mdns_discovery.h $(SHIMS_DIR)/network_discovery_protocol.cpp $(SHIMS_DIR)/network_discovery_protocol.h $(SHIMS_DIR)/vendor/mjansson_mdns/mdns.h tests/embedded_mdns_discovery_test.cpp | dirs
	@echo "=== Testing embedded mDNS fallback ==="
	@$(CXX) -std=c++17 -Wall -Wextra -Werror -I$(SHIMS_DIR) $(SHIMS_DIR)/network_discovery_protocol.cpp $(SHIMS_DIR)/embedded_mdns_discovery.cpp tests/embedded_mdns_discovery_test.cpp -o $@

test-embedded-mdns-discovery: $(EMBEDDED_MDNS_DISCOVERY_TEST)
	@unshare --user --map-root-user --net sh -c 'set -eu; \
	  ip link set lo up; \
	  ip link add dummy0 type dummy; \
	  ip link set dummy0 multicast on; \
	  ip addr add 192.0.2.1/24 dev dummy0; \
	  ip link set dummy0 up; \
	  "$(EMBEDDED_MDNS_DISCOVERY_TEST)"'

SDL3_NET_DISCOVERY_TRANSPORT_TEST := $(BUILD_DIR)/sdl3_net_discovery_transport_test

$(SDL3_NET_DISCOVERY_TRANSPORT_TEST): $(SHIMS_DIR)/embedded_mdns_discovery.cpp $(SHIMS_DIR)/network_discovery_protocol.cpp $(SHIMS_DIR)/sdl3_net_protocol.cpp $(SHIMS_DIR)/sdl3_net_transport.cpp tests/sdl3_net_discovery_transport_test.cpp | dirs
	@echo "=== Building DNS-SD to SDL_net cross-process integration ==="
	@$(CXX) -std=c++17 -Wall -Wextra -Werror -pthread -I$(SHIMS_DIR) $(SDL3_CFLAGS) $(SDL3_NET_CFLAGS) $(SHIMS_DIR)/network_discovery_protocol.cpp $(SHIMS_DIR)/embedded_mdns_discovery.cpp $(SHIMS_DIR)/sdl3_net_protocol.cpp $(SHIMS_DIR)/sdl3_net_transport.cpp tests/sdl3_net_discovery_transport_test.cpp $(SDL3_NET_LDFLAGS) $(SDL3_NET_LIBS) $(SDL3_LDFLAGS) $(SDL3_LIBS) -o $@

test-sdl3-net-discovery-transport: $(SDL3_NET_DISCOVERY_TRANSPORT_TEST)
	@SDL3_LIB="$(SDL3_LIB)"; SDL3_NET_LIB="$(SDL3_NET_LIB)"; export LD_LIBRARY_PATH="$$SDL3_NET_LIB:$$SDL3_LIB:$$LD_LIBRARY_PATH"; \
	  unshare --user --map-root-user --net sh -c 'set -eu; \
	  ip link set lo up; \
	  ip link add dummy0 type dummy; \
	  ip link set dummy0 multicast on; \
	  ip addr add 192.0.2.1/24 dev dummy0; \
	  ip link set dummy0 up; \
	  tests/sdl3_net_discovery_transport_test.sh "$(SDL3_NET_DISCOVERY_TRANSPORT_TEST)"'

# Link
$(BINARY): $(ALL_OBJS) | dirs
	$(CXX) -std=c++17 -no-pie $(ALL_OBJS) $(SDL3_LDFLAGS) $(SDL3_LIBS) $(SDL3_NET_LDFLAGS) $(SDL3_NET_LIBS) $(GST_LIBS) $(FREETYPE_LIBS) $(DBUS_LIBS) -pthread -Wl,--allow-multiple-definition -Wl,--unresolved-symbols=ignore-all -o $@

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

# PostBag cleanup recovered at 0x443470 / 0x443550.
POSTBAG_CLEANUP_TEST := $(BUILD_DIR)/postbag_cleanup_test

$(POSTBAG_CLEANUP_TEST): $(DCP_DIR)/network/PostBagCleanup.cpp tests/postbag_cleanup_test.cpp | dirs
	@echo "=== Testing PostBag cleanup ==="
	@$(CXX) -std=c++17 -Wall -Wextra -Werror $(DCP_DIR)/network/PostBagCleanup.cpp tests/postbag_cleanup_test.cpp -o $@

test-postbag-cleanup: $(POSTBAG_CLEANUP_TEST)
	@$(POSTBAG_CLEANUP_TEST)

# CGWND_EnterMode3(2) safe early return and symbol ownership regression.
# Links against real CGWND.o; only g_game_mode is provided — all other
# undefined symbols are ignored, isolating the mode-2 branch contract.
CGWND_ENTERMODE3_TEST := $(BUILD_DIR)/cgwnd_entermode3_test

$(CGWND_ENTERMODE3_TEST): $(BUILD_DIR)/dcp/core/CGWND.o tests/cgwnd_entermode3_test.cpp | dirs
	@echo "=== Testing CGWND_EnterMode3(2) safe early return ==="
	@$(CXX) -std=c++17 -Wall -Wextra -Werror -no-pie -I$(DCP_DIR) -I$(DCP_DIR)/shared -I$(DCP_DIR)/stubs $(FORCE_INC) tests/cgwnd_entermode3_test.cpp $(BUILD_DIR)/dcp/core/CGWND.o -Wl,--unresolved-symbols=ignore-all -o $@

test-cgwnd-entermode3: $(CGWND_ENTERMODE3_TEST)
	@$(CGWND_ENTERMODE3_TEST)


# Regression for lego_loco-3.core: GameLoop_Setup must create both host
# mode-3 frame dependencies before their unconditional first-frame dispatch.
HOST_MODE3_BOOTSTRAP_TEST := $(BUILD_DIR)/host_mode3_bootstrap_test

$(HOST_MODE3_BOOTSTRAP_TEST): $(BUILD_DIR)/shims/sdl3_town_mode3.o $(SHIMS_DIR)/sdl3_town_mode3.h tests/host_mode3_bootstrap_test.cpp | dirs
	@echo "=== Testing mode-3 frame dependency bootstrap ==="
	@$(CXX) -std=c++17 -Wall -Wextra -Werror -no-pie -I$(DCP_DIR) -I$(DCP_DIR)/shared -I$(DCP_DIR)/stubs -I$(SHIMS_DIR) $(FORCE_INC) tests/host_mode3_bootstrap_test.cpp $(BUILD_DIR)/shims/sdl3_town_mode3.o -Wl,--unresolved-symbols=ignore-all -o $@

test-host-mode3-bootstrap: $(HOST_MODE3_BOOTSTRAP_TEST)
	@$(HOST_MODE3_BOOTSTRAP_TEST)

# Canonical InputMgr regression: ctor 0x41D250 / dtor body 0x41D2D0 / embedded
# entity-collection ops (vtable family 0x477798/0x477758) / ResetWorldState
# Canonical InputMgr regression: ctor 0x41D250 / dtor body 0x41D2D0 / embedded
# entity-collection ops (vtable family 0x477798/0x477758) / INPUT_GetSaveFileName
# 0x41DD40 / ResetWorldState 0x41E100.  Links the real InputMgr.o; the test
# supplies g_game, operator_new, GLOBAL_free and a fail-loud
# Game::DeselectGameObject fixture.  No --unresolved-symbols=ignore-all: the
# link fails loudly if InputMgr.o references any symbol the test does not
# truthfully provide.
INPUTMGR_CANONICAL_TEST := $(BUILD_DIR)/inputmgr_canonical_test
PERSISTENCE_CONE := $(BUILD_DIR)/dcp/core/GameObject.o $(BUILD_DIR)/dcp/core/Entity.o $(BUILD_DIR)/dcp/core/BuildingMgrObjectGroup.o $(BUILD_DIR)/dcp/game/ResdataGameVehicle.o $(BUILD_DIR)/dcp/game/GameVehicle.o $(BUILD_DIR)/dcp/ui/HelpPageNode.o $(BUILD_DIR)/dcp/game/Building.o
PERSISTENCE_OBJS := $(BUILD_DIR)/dcp/input/InputMgr.o $(BUILD_DIR)/dcp/resources/ResDataSave.o $(BUILD_DIR)/dcp/input/PersistenceAdapter.o

$(INPUTMGR_CANONICAL_TEST): $(PERSISTENCE_OBJS) $(PERSISTENCE_CONE) tests/inputmgr_canonical_test.cpp tests/persistence_fixtures.h | dirs
	@echo "=== Testing canonical InputMgr (0x41D250/0x41D2D0/0x41E100) ==="
	@$(CXX) -std=c++17 -Wall -Wextra -Werror -I$(DCP_DIR) -I$(DCP_DIR)/shared -I$(DCP_DIR)/stubs -Itests $(FORCE_INC) tests/inputmgr_canonical_test.cpp $(PERSISTENCE_OBJS) $(PERSISTENCE_CONE) -o $@

test-inputmgr-canonical: $(INPUTMGR_CANONICAL_TEST)
	@$(INPUTMGR_CANONICAL_TEST)

# Persistence adapter strict parse/write regressions (shipped fixtures).
PERSISTENCE_ADAPTER_TEST := $(BUILD_DIR)/persistence_adapter_test

$(PERSISTENCE_ADAPTER_TEST): $(BUILD_DIR)/dcp/input/PersistenceAdapter.o tests/persistence_adapter_test.cpp tests/persistence_fixtures.h | dirs
	@echo "=== Testing persistence adapter (strict .loco parse/write) ==="
	@$(CXX) -std=c++17 -Wall -Wextra -Werror -I$(DCP_DIR) -I$(DCP_DIR)/shared -I$(DCP_DIR)/stubs -Itests $(FORCE_INC) tests/persistence_adapter_test.cpp $(BUILD_DIR)/dcp/input/PersistenceAdapter.o -o $@

test-persistence-adapter: $(PERSISTENCE_ADAPTER_TEST)
	@cd $(PROJECT_ROOT) && $(PERSISTENCE_ADAPTER_TEST)

# INPUT_* world new/load/save + typed callees regressions (shipped fixtures).
INPUT_WORLD_TEST := $(BUILD_DIR)/input_world_test

$(INPUT_WORLD_TEST): $(PERSISTENCE_OBJS) $(PERSISTENCE_CONE) tests/input_world_test.cpp tests/persistence_fixtures.h | dirs
	@echo "=== Testing INPUT_NewWorld/LoadWorld/LoadSaveFile/SaveCurrentWorld ==="
	@$(CXX) -std=c++17 -Wall -Wextra -Werror -I$(DCP_DIR) -I$(DCP_DIR)/shared -I$(DCP_DIR)/stubs -Itests $(FORCE_INC) tests/input_world_test.cpp $(PERSISTENCE_OBJS) $(PERSISTENCE_CONE) -o $@

test-input-world: $(INPUT_WORLD_TEST)
	@cd $(PROJECT_ROOT) && $(INPUT_WORLD_TEST)

# Original MCI launch order recovered from 0x421EB0 / 0x420F7F.
INTRO_VIDEO_SEQUENCE_TEST := $(BUILD_DIR)/intro_video_sequence_test

$(INTRO_VIDEO_SEQUENCE_TEST): $(SHIMS_DIR)/sdl3_intro_video.h tests/intro_video_sequence_test.cpp | dirs
	@echo "=== Testing original intro-video order ==="
	@$(CXX) -std=c++17 -Wall -Wextra -Werror -I$(SHIMS_DIR) tests/intro_video_sequence_test.cpp -o $@

test-intro-video-sequence: $(INTRO_VIDEO_SEQUENCE_TEST)
	@$(INTRO_VIDEO_SEQUENCE_TEST)


# Link regression: a missing intro-player object leaves startLaunchSequence()
# unresolved, and --unresolved-symbols=ignore-all turns the main.cpp call into
# a RIP-0 crash immediately after GameLoop_Setup (lego_loco-4.core).
test-host-intro-video-linkage: $(BINARY) tests/host_intro_video_linkage_test.sh
	@bash tests/host_intro_video_linkage_test.sh $(BINARY)

# SDL primary-target regression: validates the CGWND frame source reaches the window.
SDL3_PRIMARY_PRESENT_TEST := $(BUILD_DIR)/sdl3_primary_present_test

$(SDL3_PRIMARY_PRESENT_TEST): $(SHIMS_DIR)/sdl3_window.cpp $(SHIMS_DIR)/sdl3_window.h $(SHIMS_DIR)/sdl3_ddraw.cpp $(SHIMS_DIR)/sdl3_ddraw.h $(SHIMS_DIR)/sdl3_game_audio.cpp $(SHIMS_DIR)/sdl3_game_audio.h $(SHIMS_DIR)/resource_manager_sdl3.cpp $(SHIMS_DIR)/resource_manager_sdl3.h $(SHIMS_DIR)/resource_archive.cpp $(SHIMS_DIR)/resource_archive.h $(SHIMS_DIR)/pe_string_table.cpp $(SHIMS_DIR)/pe_string_table.h $(SHIMS_DIR)/host_test_events.cpp $(SHIMS_DIR)/host_test_events.h tests/sdl3_primary_present_test.cpp | dirs
	@echo "=== Testing SDL primary-surface presentation ==="
	@$(CXX) -std=c++17 -Wall -Wextra -Werror $(FORCE_INC) $(SDL3_CFLAGS) $(SHIMS_DIR)/sdl3_window.cpp $(SHIMS_DIR)/sdl3_ddraw.cpp $(SHIMS_DIR)/sdl3_game_audio.cpp $(SHIMS_DIR)/resource_manager_sdl3.cpp $(SHIMS_DIR)/resource_archive.cpp $(SHIMS_DIR)/pe_string_table.cpp $(SHIMS_DIR)/host_test_events.cpp tests/sdl3_primary_present_test.cpp $(SDL3_LDFLAGS) $(SDL3_LIBS) -o $@

test-sdl3-primary-present: $(SDL3_PRIMARY_PRESENT_TEST)
	@SDL3_LIB="$(SDL3_LIB)"; if [ -n "$$SDL3_LIB" ]; then export LD_LIBRARY_PATH="$$SDL3_LIB:$$LD_LIBRARY_PATH"; fi; SDL_VIDEODRIVER=dummy $(SDL3_PRIMARY_PRESENT_TEST)

# Mode-2 EditWindow::render regression: recovered backdrop resources reach the SDL primary target.
MODE2_MENU_BACKDROP_TEST := $(BUILD_DIR)/mode2_menu_backdrop_test

$(MODE2_MENU_BACKDROP_TEST): $(SHIMS_DIR)/resource_archive.cpp $(SHIMS_DIR)/resource_archive.h $(SHIMS_DIR)/pe_string_table.cpp $(SHIMS_DIR)/pe_string_table.h $(SHIMS_DIR)/resource_manager_sdl3.cpp $(SHIMS_DIR)/resource_manager_sdl3.h $(SHIMS_DIR)/sdl3_window.cpp $(SHIMS_DIR)/sdl3_window.h $(SHIMS_DIR)/sdl3_ddraw.cpp $(SHIMS_DIR)/sdl3_ddraw.h $(SHIMS_DIR)/sdl3_game_audio.cpp $(SHIMS_DIR)/sdl3_game_audio.h $(SHIMS_DIR)/host_test_events.cpp $(SHIMS_DIR)/host_test_events.h tests/mode2_menu_backdrop_test.cpp | dirs
	@echo "=== Testing mode 2 EditWindow backdrop composition ==="
	@$(CXX) -std=c++17 -Wall -Wextra -Werror $(FORCE_INC) $(SDL3_CFLAGS) $(SHIMS_DIR)/resource_archive.cpp $(SHIMS_DIR)/pe_string_table.cpp $(SHIMS_DIR)/resource_manager_sdl3.cpp $(SHIMS_DIR)/sdl3_window.cpp $(SHIMS_DIR)/sdl3_ddraw.cpp $(SHIMS_DIR)/sdl3_game_audio.cpp $(SHIMS_DIR)/host_test_events.cpp tests/mode2_menu_backdrop_test.cpp $(SDL3_LDFLAGS) $(SDL3_LIBS) -o $@

test-mode2-menu-backdrop: $(MODE2_MENU_BACKDROP_TEST)
	@SDL3_LIB="$(SDL3_LIB)"; if [ -n "$$SDL3_LIB" ]; then export LD_LIBRARY_PATH="$$SDL3_LIB:$$LD_LIBRARY_PATH"; fi; cd $(PROJECT_ROOT) && SDL_VIDEODRIVER=dummy $(MODE2_MENU_BACKDROP_TEST)

# Mode-2 multiplayer GameSetupPanel host-composition regression.
MODE2_MULTIPLAYER_MENU_TEST := $(BUILD_DIR)/mode2_multiplayer_menu_test

$(MODE2_MULTIPLAYER_MENU_TEST): $(SHIMS_DIR)/resource_archive.cpp $(SHIMS_DIR)/resource_archive.h $(SHIMS_DIR)/pe_string_table.cpp $(SHIMS_DIR)/pe_string_table.h $(SHIMS_DIR)/resource_manager_sdl3.cpp $(SHIMS_DIR)/resource_manager_sdl3.h $(SHIMS_DIR)/sdl3_window.cpp $(SHIMS_DIR)/sdl3_window.h $(SHIMS_DIR)/sdl3_ddraw.cpp $(SHIMS_DIR)/sdl3_ddraw.h $(SHIMS_DIR)/sdl3_game_audio.cpp $(SHIMS_DIR)/sdl3_game_audio.h $(SHIMS_DIR)/host_test_events.cpp $(SHIMS_DIR)/host_test_events.h tests/mode2_multiplayer_menu_test.cpp | dirs
	@echo "=== Testing mode 2 multiplayer menu composition ==="
	@$(CXX) -std=c++17 -Wall -Wextra -Werror $(FORCE_INC) $(SDL3_CFLAGS) $(SHIMS_DIR)/resource_archive.cpp $(SHIMS_DIR)/pe_string_table.cpp $(SHIMS_DIR)/resource_manager_sdl3.cpp $(SHIMS_DIR)/sdl3_window.cpp $(SHIMS_DIR)/sdl3_ddraw.cpp $(SHIMS_DIR)/sdl3_game_audio.cpp $(SHIMS_DIR)/host_test_events.cpp tests/mode2_multiplayer_menu_test.cpp $(SDL3_LDFLAGS) $(SDL3_LIBS) -o $@

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

# Regression for both recovered startup menu choices: singleup and multipleup.
test-host-multiplayer-selector: tests/host_multiplayer_selector_test.sh
	@tests/host_multiplayer_selector_test.sh

# Regression for host-only routing of SDL clicks into GameSetupPanel control adapter.
test-host-multiplayer-menu-input: tests/host_multiplayer_menu_input_test.sh
	@tests/host_multiplayer_menu_input_test.sh

# Host sound regression: mode 2 preloads 0x5015; mode 10 plays 0x5026.
SDL3_GAME_AUDIO_TEST := $(BUILD_DIR)/sdl3_game_audio_test

$(SDL3_GAME_AUDIO_TEST): $(SHIMS_DIR)/resource_archive.cpp $(SHIMS_DIR)/resource_archive.h $(SHIMS_DIR)/pe_string_table.cpp $(SHIMS_DIR)/pe_string_table.h $(SHIMS_DIR)/resource_manager_sdl3.cpp $(SHIMS_DIR)/resource_manager_sdl3.h $(SHIMS_DIR)/sdl3_game_audio.cpp $(SHIMS_DIR)/sdl3_game_audio.h $(SHIMS_DIR)/host_test_events.cpp $(SHIMS_DIR)/host_test_events.h tests/sdl3_game_audio_test.cpp | dirs
	@echo "=== Testing host game audio ==="
	@$(CXX) -std=c++17 -Wall -Wextra -Werror $(SDL3_CFLAGS) $(SHIMS_DIR)/resource_archive.cpp $(SHIMS_DIR)/pe_string_table.cpp $(SHIMS_DIR)/resource_manager_sdl3.cpp $(SHIMS_DIR)/host_test_events.cpp $(SHIMS_DIR)/sdl3_game_audio.cpp tests/sdl3_game_audio_test.cpp $(SDL3_LDFLAGS) $(SDL3_LIBS) -o $@

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
	@echo "  make diagnostic-census Clean/-k full build in a temporary tree and count diagnostics"
	@echo "  make clean            Remove generated build outputs"
	@echo "  make distclean Reset everything"
	@echo "  make check    Show status"


_test:
	@echo "PROJECT_ROOT=[$(PROJECT_ROOT)]"
	@echo "BUILD_DIR=[$(BUILD_DIR)]"
	@echo "SHIMS_DIR=[$(SHIMS_DIR)]"
	@echo "SHIM_SRCS=[$(SHIM_SRCS)]"
	@echo "SHIM_OBJS=[$(SHIM_OBJS)]"
