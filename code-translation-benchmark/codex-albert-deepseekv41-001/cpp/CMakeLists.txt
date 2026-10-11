cmake_minimum_required(VERSION 3.10)
project(flora CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

add_compile_options(-Wall -Wextra)

# SDL2 via pkg-config first; fall back to find_package.
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(SDL2 QUIET sdl2)
endif()

if(SDL2_FOUND)
  set(FLORA_SDL2_INCLUDE ${SDL2_INCLUDE_DIRS})
  set(FLORA_SDL2_LIB ${SDL2_LIBRARIES})
  set(FLORA_SDL2_LIBDIR ${SDL2_LIBRARY_DIRS})
  set(FLORA_SDL2_CFLAGS ${SDL2_CFLAGS_OTHER})
else()
  find_package(SDL2 REQUIRED)
  if(TARGET SDL2::SDL2)
    set(FLORA_SDL2_LIB SDL2::SDL2)
  else()
    set(FLORA_SDL2_INCLUDE ${SDL2_INCLUDE_DIRS})
    set(FLORA_SDL2_LIB ${SDL2_LIBRARIES})
  endif()
  set(FLORA_SDL2_LIBDIR ${SDL2_LIBRARY_DIRS})
endif()

set(FLORA_SOURCES
  src/main.cpp
  src/clock.cpp
  src/easing.cpp
  src/ctm.cpp
  src/panel.cpp
  src/assets.cpp
  src/render.cpp
)

add_executable(flora ${FLORA_SOURCES})

target_include_directories(flora PRIVATE
  ${FLORA_SDL2_INCLUDE}
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

if(FLORA_SDL2_LIBDIR)
  target_link_directories(flora PRIVATE ${FLORA_SDL2_LIBDIR})
endif()

target_link_libraries(flora PRIVATE ${FLORA_SDL2_LIB} m)

if(FLORA_SDL2_CFLAGS)
  target_compile_options(flora PRIVATE ${FLORA_SDL2_CFLAGS})
endif()

# The vendored stb headers are noisy under -Wall -Wextra; silence that TU only.
set_source_files_properties(src/assets.cpp PROPERTIES COMPILE_OPTIONS "-w")

# Java-style 32-bit integer wraparound is relied on by the rasteriser.
target_compile_options(flora PRIVATE -fwrapv)
