cmake_minimum_required(VERSION 3.16)

project(
  minecraft-carve
  VERSION 0.1.0
  DESCRIPTION "Recover minecraft region files and other data from a filesystem image"
  HOMEPAGE_URL "https://github.com/maspitz/minecraft-carve"
  LANGUAGES CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)

find_package(ZLIB REQUIRED)

add_executable(mcarve-pass1
    apps/mcarve-pass1.cpp
    source/pass1config.cpp
)

target_link_libraries(mcarve-pass1 ${ZLIB_LIBRARIES})

target_include_directories(mcarve-pass1 PRIVATE include)
