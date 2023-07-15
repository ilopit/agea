file(GLOB LIB_SRC "public/include/render_bridge/*.h" "private/src/*.cpp")

add_library(render_bridge STATIC
   ${LIB_SRC}
)

target_link_libraries(render_bridge

    agea::root.module.model
    agea::utils
    agea::vulkan_render
)

agea_finalize_library(render_bridge)

add_subdirectory(private/tests)