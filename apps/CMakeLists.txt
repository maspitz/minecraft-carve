add_executable(mcarve
    mcarve.cpp
)

target_link_libraries(mcarve minecraft-carve)
target_include_directories(mcarve PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/nbtview)

add_executable(dump_unused_blocks
    dump_unused_blocks.cpp
)

target_link_libraries(dump_unused_blocks minecraft-carve)
target_include_directories(dump_unused_blocks PRIVATE ${PROJECT_SOURCE_DIR})
