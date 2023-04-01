file(GLOB LIB_SRC "include/engine/*.h" "src/*.cpp")

add_library(engine STATIC
   ${LIB_SRC}
)
target_link_libraries(engine PUBLIC
    sdl2

    agea::utils
    agea::model
    agea::vulkan_render
    agea::asset_importer

    demo
)

agea_finalize_library(engine)