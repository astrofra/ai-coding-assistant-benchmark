cmake_minimum_required(VERSION 3.10)
project(flora CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(SDL2 REQUIRED)

add_executable(flora
  src/main.cpp
  src/clock.cpp
  src/easing.cpp
  src/ctm.cpp
  src/surface.cpp
  src/assets.cpp
  src/panel.cpp
  third_party/stb_impl.cpp
)

target_include_directories(flora PRIVATE src third_party ${SDL2_INCLUDE_DIRS})
target_compile_options(flora PRIVATE -Wall -Wextra -O2)
target_link_libraries(flora PRIVATE ${SDL2_LIBRARIES} m)

