cmake_minimum_required(VERSION 3.16)
project(lego-loco-sdl3-shims
    VERSION 0.1.0
    DESCRIPTION "SDL3 compatibility shims for Lego Loco DirectX APIs"
    LANGUAGES CXX
)

option(BUILD_SHIMS_ONLY "Build only the SDL3 shim library (no test executable)" ON)

find_package(SDL3 REQUIRED)
find_package(SDL3_image QUIET)

add_library(loco_sdl3_shims STATIC
    sdl3_ddraw.cpp
    sdl3_dsound.cpp
    sdl3_window.cpp
)

target_include_directories(loco_sdl3_shims PUBLIC .)
target_link_libraries(loco_sdl3_shims PUBLIC SDL3::SDL3)

if(SDL3_image_FOUND)
    target_compile_definitions(loco_sdl3_shims PRIVATE HAVE_SDL3_IMAGE)
    target_link_libraries(loco_sdl3_shims PUBLIC SDL3_image::SDL3_image)
endif()

target_compile_features(loco_sdl3_shims PUBLIC cxx_std_17)
