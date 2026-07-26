# Makefile — SDL3 DirectX shims for Lego Loco
#
# Builds the SDL3 shim objects for linking with decompiled C++ code.
#
# Usage:
#   make                    — build shim objects
#   make check              — verify shims compile
#   make clean              — remove build artifacts

CXX      := g++
CXXFLAGS := -std=c++17 -c -Wall -Wextra -Wno-unused-parameter

# Auto-detect SDL3 from Nix store
SDL3_DEV  := $(shell ls -d /nix/store/*sdl3-*-dev 2>/dev/null | head -1)
SDL3_LIB  := $(shell ls -d /nix/store/*sdl3-*-lib/lib 2>/dev/null | head -1)

ifneq ($(SDL3_DEV),)
  SDL3_INC := -I$(SDL3_DEV)/include
else
  SDL3_INC := $(shell pkg-config --cflags sdl3 2>/dev/null)
endif

ifneq ($(SDL3_LIB),)
  SDL3_LDFLAGS := -L$(SDL3_LIB) -lSDL3
else
  SDL3_LDFLAGS := $(shell pkg-config --libs sdl3 2>/dev/null)
endif

# SDL3_image (optional)
SDL3_IMG_DEV := $(shell ls -d /nix/store/*sdl3-image-*-dev 2>/dev/null | head -1)
SDL3_IMG_LIB := $(shell ls -d /nix/store/*sdl3-image-*-lib/lib 2>/dev/null | head -1)
ifneq ($(SDL3_IMG_DEV),)
  SDL3_IMG_INC := -I$(SDL3_IMG_DEV)/include -DHAVE_SDL3_IMAGE
endif
ifneq ($(SDL3_IMG_LIB),)
  SDL3_IMG_LDFLAGS := -L$(SDL3_IMG_LIB) -lSDL3_image
endif

override CXXFLAGS += $(SDL3_INC) $(SDL3_IMG_INC) -include ../decompiled_cpp/stubs/compat.h -I../decompiled_cpp -I../decompiled_cpp/stubs

BUILDDIR := build
SRCS     := sdl3_ddraw.cpp sdl3_dsound.cpp sdl3_window.cpp
OBJS     := $(patsubst %.cpp,$(BUILDDIR)/%.o,$(SRCS))

.PHONY: all check clean

all: $(OBJS)

$(BUILDDIR)/%.o: %.cpp sdl3_types.h sdl3_ddraw.h sdl3_dsound.h sdl3_dplay.h
	@mkdir -p $(BUILDDIR)
	@echo "  CXX  $<"
	@$(CXX) $(CXXFLAGS) $< -o $@ 2>$@.err; ret=$$?; 	if [ $$ret -ne 0 ]; then echo "FAILED:"; head -20 $@.err; rm -f $@; exit 1; fi; 	rm -f $@.err

check:
	@echo "=== SDL3 Shim Compilation Check ==="
	@echo "SDL3 include: $(SDL3_INC)"
	@echo "SDL3 lib:     $(SDL3_LDFLAGS)"
	@echo ""
	@for f in $(SRCS); do 		printf "  [....] %s" "$$f"; 		if $(CXX) $(CXXFLAGS) $$f -o /dev/null 2>/dev/null; then 			printf "\r  [OK]   %s\n" "$$f"; 		else 			printf "\r  [FAIL] %s\n" "$$f"; 			$(CXX) $(CXXFLAGS) $$f -o /dev/null 2>&1 | head -5; 		fi; 	done
	@echo ""
	@echo "=== Done ==="

clean:
	rm -rf $(BUILDDIR)
