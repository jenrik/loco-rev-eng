cmake_minimum_required(VERSION 3.16)
project(lego-loco-port C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)

# Find SDL2 and companions
find_package(SDL2 REQUIRED)
find_package(SDL2_mixer REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_ttf REQUIRED)

# Optional: GStreamer for AVI video playback
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST gstreamer-1.0 gstreamer-video-1.0)

# Optional: ENet for DirectPlay-replacement multiplayer
find_library(ENET_LIB enet)

# inih — drop-in INI parser replacing GetPrivateProfileStringA
add_library(inih STATIC third_party/inih/ini.c)
target_include_directories(inih PUBLIC third_party/inih)

# Main executable — Lego Loco native Linux port
add_executable(lego-loco
    src/main.c
    src/core/core.c
    src/platform/sdl_window.c
    src/platform/sdl_audio.c
    src/platform/sdl_input.c
    src/platform/sdl_video.c
    src/platform/ini_config.c
    src/graphics/locobitmap.c
    src/graphics/ddraw_init.c
    src/audio/audio.c
    src/network/netman.c
    src/resources/resources.c
    src/resources/huffman.c
    src/game/game_world.c
    src/game/tile_desc.c
    src/input/input.c
    src/ui/ui.c
    src/platform/win32_platform.c
)

target_compile_definitions(lego-loco PRIVATE
    LOCO_LINUX=1
    LOCO_VERSION_MAJOR=1
    LOCO_VERSION_MINOR=0
)

target_include_directories(lego-loco PRIVATE
    src
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_MIXER_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
    ${SDL2_TTF_INCLUDE_DIRS}
)

target_link_libraries(lego-loco PRIVATE
    ${SDL2_LIBRARIES}
    ${SDL2_MIXER_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
    inih
    m
)

if(GST_FOUND)
    target_compile_definitions(lego-loco PRIVATE LOCO_USE_GSTREAMER=1)
    target_include_directories(lego-loco PRIVATE ${GST_INCLUDE_DIRS})
    target_link_libraries(lego-loco PRIVATE ${GST_LIBRARIES})
    message(STATUS "GStreamer found — AVI video playback enabled")
else()
    message(STATUS "GStreamer not found — AVI video playback disabled")
endif()

if(ENET_LIB)
    target_compile_definitions(lego-loco PRIVATE LOCO_USE_ENET=1)
    target_link_libraries(lego-loco PRIVATE ${ENET_LIB})
    message(STATUS "ENet found — DirectPlay-compatible multiplayer enabled")
else()
    message(STATUS "ENet not found — multiplayer disabled")
endif()

install(TARGETS lego-loco RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/data/ DESTINATION share/lego-loco)
