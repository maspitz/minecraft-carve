add_executable(mcarve
    mcarve.cpp
    mcarve-config.cpp
)

target_link_libraries(mcarve minecraft-carve)
target_include_directories(mcarve PRIVATE ${CMAKE_SOURCE_DIR}/CLI11)
