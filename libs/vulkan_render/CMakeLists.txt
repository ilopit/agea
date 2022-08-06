set(CMAKE_CXX_STANDARD 17)

file(GLOB LIB_SRC          "include/vulkan_render/*.h"              "src/*.cpp")
file(GLOB DATA_LOADERS_SRC "include/vulkan_render/data_loaders/*.h" "src/data_loaders/*.cpp")

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
    agea::model_global_api
    agea::serialization

    tinyobjloader
    sdl2
)
agea_finalize_library(vulkan_render)