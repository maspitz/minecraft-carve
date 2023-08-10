find_package(PkgConfig REQUIRED)

pkg_check_modules(EXT2FS REQUIRED ext2fs)
pkg_check_modules(COM_ERR REQUIRED com_err)

add_library(minecraft-carve STATIC ext2filesystem.cpp sector.cpp)

target_link_libraries(minecraft-carve ${E2P_LIBRARIES} ${COM_ERR_LIBRARIES} ${EXT2FS_LIBRARIES})
target_include_directories(minecraft-carve
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${E2P_INCLUDE_DIRS} ${EXT2FS_INCLUDE_DIRS})

install(TARGETS minecraft-carve DESTINATION lib)

install(FILES ext2filesystem.hpp sector.hpp DESTINATION include)
