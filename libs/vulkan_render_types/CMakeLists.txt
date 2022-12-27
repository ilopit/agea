file(GLOB LIB_SRC "include/vulkan_render_types/*.h" "src/*.cpp")

add_library(vulkan_render_types STATIC
   ${LIB_SRC}
   ${DATA_LOADERS_SRC}
)

target_link_libraries(vulkan_render_types PUBLIC
    agea::utils
    agea::glm_unofficial
    agea::vma_unofficial

    Vulkan::Vulkan
    vk-bootstrap::vk-bootstrap
    spirv-reflect-static
)

agea_finalize_library(vulkan_render_types)