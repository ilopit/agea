file(GLOB CORE_SRC         "public/include/vulkan_render/*.h"                "private/src/*.cpp")
file(GLOB VULKAN_LOADERS   "public/include/vulkan_render/vulkan_loaders/*.h" "private/src/vulkan_loaders/*.cpp")
file(GLOB UTILS            "public/include/vulkan_render/utils/*.h"          "private/src/utils/*.cpp")
file(GLOB TYPES            "public/include/vulkan_render/types/*.h"          "private/src/types/*.cpp")
file(GLOB BAKE             "public/include/vulkan_render/bake/*.h"           "private/src/bake/*.cpp")

source_group("core"    FILES  ${CORE_SRC})
source_group("loaders" FILES  ${VULKAN_LOADERS})
source_group("utils"   FILES  ${UTILS})
source_group("types"   FILES  ${TYPES})
source_group("bake"    FILES  ${BAKE})

add_library(vulkan_render STATIC
    ${CORE_SRC}
    ${VULKAN_LOADERS}
    ${UTILS}
    ${TYPES}
    ${BAKE}
)

target_link_libraries(vulkan_render PUBLIC
    kryga::utils
    kryga::gpu_types
    kryga::native

    kryga::vfs
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

# Vulkan validation layers, debug messenger, and object naming.
# ON by default in Debug/RelWithDebInfo, OFF in Release/MinSizeRel.
# Override: -DKRYGA_VULKAN_DEBUG=1 (or 0) on the cmake command line.
if(NOT DEFINED KRYGA_VULKAN_DEBUG)
    target_compile_definitions(vulkan_render PUBLIC
        $<$<CONFIG:Debug>:KRYGA_VULKAN_DEBUG=1>
        $<$<CONFIG:RelWithDebInfo>:KRYGA_VULKAN_DEBUG=1>
        $<$<CONFIG:Release>:KRYGA_VULKAN_DEBUG=0>
        $<$<CONFIG:MinSizeRel>:KRYGA_VULKAN_DEBUG=0>
    )
else()
    target_compile_definitions(vulkan_render PUBLIC
        KRYGA_VULKAN_DEBUG=$<BOOL:${KRYGA_VULKAN_DEBUG}>
    )
endif()

kryga_finalize_library(vulkan_render)

add_subdirectory(private/tests)