set(CMAKE_CXX_STANDARD 17)

file(GLOB LIB_SRC "include/native/*.h" "src/*.cpp")

add_library(native STATIC
   ${LIB_SRC}
)

target_link_libraries(native
    agea::utils
    sdl2
)

agea_finalize_library(native)