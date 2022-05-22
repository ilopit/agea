
set(sdl2_DIR "$ENV{SDL2_PATH}")

add_library(sdl2 INTERFACE)

target_include_directories(sdl2 INTERFACE "${sdl2_DIR}/include")
target_link_directories(sdl2 INTERFACE "${sdl2_DIR}/lib/x64")
target_link_libraries(sdl2 INTERFACE SDL2 SDL2main)

configure_file(
    "${sdl2_DIR}/lib/x64/SDL2.dll"
    "${CMAKE_BINARY_DIR}/project_${CMAKE_BUILD_TYPE}/bin/SDL2.dll" COPYONLY)