set(CMAKE_CXX_STANDARD 17)

file(GLOB LIB_SRC "include/engine/*.h" "src/*.cpp")

add_library(engine STATIC
   ${LIB_SRC}
)
target_link_libraries(engine PUBLIC
    sdl2

    agea::utils
    agea::model
    agea::vulkan_render
)

agea_finalize_library(engine)