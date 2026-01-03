file(GLOB LIB_SRC "public/include/render_bridge/*.h" "private/src/*.cpp")

add_library(render_bridge STATIC
   ${LIB_SRC}
)

target_link_libraries(render_bridge

    kryga::packages.root.model
    kryga::utils
    kryga::vulkan_render
)

kryga_finalize_library(render_bridge)

add_subdirectory(private/tests)