cmake_minimum_required(VERSION 3.15)
project(flora_calendar LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# SDL2 via pkg-config, falling back to any FindSDL2 module.
find_package(PkgConfig QUIET)
set(flora_sdl "")
if(PkgConfig_FOUND)
    pkg_check_modules(SDL2 QUIET IMPORTED_TARGET sdl2)
    if(SDL2_FOUND)
        set(flora_sdl PkgConfig::SDL2)
    endif()
endif()
if(NOT flora_sdl)
    find_package(SDL2 REQUIRED)
    set(flora_sdl SDL2::SDL2)
endif()

# Compile each vendored stb header once in its own translation unit.
add_library(stb_image STATIC src/stb_image.cpp)
target_include_directories(stb_image SYSTEM PUBLIC third_party)
add_library(stb_image_write STATIC src/stb_image_write.cpp)
target_include_directories(stb_image_write SYSTEM PUBLIC third_party)

add_library(flora_core STATIC
    src/cal2.cpp
    src/cal2panel.cpp
    src/IdlePanel.cpp
    src/Ctm.cpp
    src/efloat.cpp
    src/eint.cpp
    src/host.cpp
    src/assets.cpp)
target_include_directories(flora_core PUBLIC src third_party)
target_link_libraries(flora_core PRIVATE stb_image stb_image_write ${flora_sdl})

add_executable(flora src/main.cpp)
target_compile_definitions(flora PRIVATE SDL_MAIN_HANDLED)
target_link_libraries(flora PRIVATE flora_core ${flora_sdl})
set_target_properties(flora PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(flora PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${config} "${CMAKE_BINARY_DIR}/bin")
endforeach()

# Copy sprites beside the executable so the binary finds them via <exe>/assets/ims.
add_custom_target(flora_assets ALL
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets" "${CMAKE_BINARY_DIR}/bin/assets"
    COMMENT "Copying Flora sprite assets" VERBATIM)
add_dependencies(flora flora_assets)

foreach(target flora_core flora)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /fp:strict)
    else()
        # Java's float expressions must not become fused multiply-adds.
        target_compile_options(${target} PRIVATE -Wall -Wextra -ffp-contract=off)
    endif()
endforeach()
