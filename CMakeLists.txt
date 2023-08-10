cmake_minimum_required(VERSION 3.16)

project(
  minecraft-carve
  VERSION 0.1.0
  DESCRIPTION "Recover minecraft region files and other data from a filesystem image"
  HOMEPAGE_URL "https://github.com/maspitz/minecraft-carve"
  LANGUAGES CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(minecraft-carve)
add_subdirectory(apps)
