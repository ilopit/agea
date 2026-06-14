file(GLOB LIB_SRC "public/include/render_translator/*.h" "private/src/*.cpp")

add_library(render_translator STATIC
   ${LIB_SRC}
)

target_link_libraries(render_translator

    kryga::packages.root.model
    kryga::utils
    kryga::render_types
    kryga::vulkan_render
)

kryga_finalize_library(render_translator)

add_subdirectory(private/tests)