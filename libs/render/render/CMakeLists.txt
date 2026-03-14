file(GLOB CORE_SRC         "public/include/vulkan_render/*.h"                "private/src/*.cpp")
file(GLOB VULKAN_LOADERS   "public/include/vulkan_render/vulkan_loaders/*.h" "private/src/vulkan_loaders/*.cpp")
file(GLOB UTILS            "public/include/vulkan_render/utils/*.h"          "private/src/utils/*.cpp")
file(GLOB TYPES            "public/include/vulkan_render/types/*.h"          "private/src/types/*.cpp")

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
    kryga::utils
    kryga::gpu_types
    kryga::native
    kryga::resource_locator
    kryga::serialization
    kryga::error_handling
    kryga::global_state
    kryga::render_utils
    kryga::shader_system

    kryga::glm_unofficial
    kryga::imgui_unofficial
    kryga::stb_unofficial
    kryga::vma_unofficial

    Vulkan::Vulkan
    vk-bootstrap::vk-bootstrap
    spirv-reflect-static
    tinyobjloader
    sdl2

    TracyClient
)
target_precompile_headers(vulkan_render PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:<vulkan/vulkan.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<imgui.h$<ANGLE-R>>"
)

kryga_finalize_library(vulkan_render)

add_subdirectory(private/tests)