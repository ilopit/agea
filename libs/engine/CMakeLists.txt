file(GLOB LIB_SRC "include/engine/*.h" "src/*.cpp")

add_library(engine STATIC
   ${LIB_SRC}
)
target_link_libraries(engine PUBLIC
    sdl2

    agea::utils
    agea::core
    agea::vulkan_render
    agea::asset_importer
    agea::render_bridge
    agea::root.m.render

    demo
)

agea_finalize_library(engine)