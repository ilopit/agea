file(GLOB LIB_SRC          "include/vulkan_render/*.h"              "src/*.cpp")
file(GLOB DATA_LOADERS_SRC "include/vulkan_render/vulkan_loaders/*.h" "src/vulkan_loaders/*.cpp")

add_library(vulkan_render STATIC
   ${LIB_SRC}
   ${DATA_LOADERS_SRC}
)
target_link_libraries(vulkan_render PUBLIC
    agea::utils
    agea::native
    agea::vulkan_render_types
    agea::glm_unofficial
    agea::imgui_unofficial
    agea::stb_unofficial
    agea::resource_locator
    agea::serialization

    tinyobjloader
    sdl2
)
agea_finalize_library(vulkan_render)