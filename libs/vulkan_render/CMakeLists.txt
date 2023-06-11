file(GLOB CORE_SRC         "include/vulkan_render/*.h"                "src/*.cpp")
file(GLOB VULKAN_LOADERS   "include/vulkan_render/vulkan_loaders/*.h" "src/vulkan_loaders/*.cpp")
file(GLOB UTILS            "include/vulkan_render/utils/*.h"          "src/utils/*.cpp")
file(GLOB TYPES            "include/vulkan_render/types/*.h"          "src/types/*.cpp")

source_group("core"    FILES  ${CORE_SRC})
source_group("loaders" FILES  ${VULKAN_LOADERS})
source_group("utils"   FILES  ${UTILS})
source_group("types"   FILES  ${TYPES})

add_library(vulkan_render STATIC
    ${CORE_SRC}
    ${VULKAN_LOADERS}
    ${UTILS}
    ${TYPES}
)

target_link_libraries(vulkan_render PUBLIC
    agea::utils
    agea::native
    agea::resource_locator
    agea::serialization
    agea::error_handling

    agea::glm_unofficial
    agea::imgui_unofficial
    agea::stb_unofficial
    agea::vma_unofficial

    Vulkan::Vulkan
    vk-bootstrap::vk-bootstrap
    spirv-reflect-static
    tinyobjloader
    sdl2
)
agea_finalize_library(vulkan_render)

add_subdirectory(tests)