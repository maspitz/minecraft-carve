cmake_minimum_required(VERSION 3.16)

project(
  minecraft-carve
  VERSION 0.1.0
  DESCRIPTION "Recover minecraft region files and other data from a filesystem image"
  HOMEPAGE_URL "https://github.com/maspitz/minecraft-carve"
  LANGUAGES CXX
)

#set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)

find_package(ZLIB REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(EXT2FS REQUIRED ext2fs)
pkg_check_modules(COM_ERR REQUIRED com_err)

add_executable(mcarve-pass1
    apps/mcarve-pass1.cpp
    source/pass1config.cpp
    source/ext2filesystem.cpp
    source/sector.cpp
)

target_link_libraries(mcarve-pass1 ${ZLIB_LIBRARIES} ${E2P_LIBRARIES} ${COM_ERR_LIBRARIES} ${EXT2FS_LIBRARIES})
target_include_directories(mcarve-pass1 PRIVATE include ${E2P_INCLUDE_DIRS} ${EXT2FS_INCLUDE_DIRS})
