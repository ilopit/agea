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

set(KRG_VULKAN_RENDER_SRC
    ${CORE_SRC}
    ${VULKAN_LOADERS}
    ${UTILS}
    ${TYPES}
    ${BAKE}
)

# ----------------------------------------------------------------------------
# kryga::vulkan_render — INTERFACE library
# ----------------------------------------------------------------------------
# Provides the public include path and the *non-ImGui* transitive deps. This
# is what intermediate libs (animation, asset_importer, render_bridge, tests)
# link against — they get the headers but no implementation symbols. The exe
# pulls in exactly one implementation flavor (game or editor) at link time.
add_library(vulkan_render INTERFACE)
target_include_directories(vulkan_render INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/public/include"
)
target_link_libraries(vulkan_render INTERFACE
    kryga::utils
    kryga::render_types
    kryga::gpu_types
    kryga::native

    kryga::vfs
    kryga::serialization
    kryga::error_handling
    kryga::global_state
    kryga::render_utils
    kryga::shader_system

    kryga::glm_unofficial
    kryga::stb_unofficial
    kryga::vma_unofficial

    Vulkan::Vulkan
    vk-bootstrap::vk-bootstrap
    spirv-reflect-static
    tinyobjloader
    sdl2

    TracyClient
)

# Vulkan validation layers, debug messenger, and object naming.
# ON by default in Debug/RelWithDebInfo, OFF in Release/MinSizeRel.
# Forced OFF on Android because validation layers aren't bundled there and
# the emulator's goldfish_vk stubs out VK_EXT_debug_utils entry points (calling
# them aborts the process).
# Override: -DKRYGA_VULKAN_DEBUG=1 (or 0) on the cmake command line.
if(ANDROID)
    target_compile_definitions(vulkan_render INTERFACE KRYGA_VULKAN_DEBUG=0)
elseif(NOT DEFINED KRYGA_VULKAN_DEBUG)
    target_compile_definitions(vulkan_render INTERFACE
        $<$<CONFIG:Debug>:KRYGA_VULKAN_DEBUG=1>
        $<$<CONFIG:RelWithDebInfo>:KRYGA_VULKAN_DEBUG=1>
        $<$<CONFIG:Release>:KRYGA_VULKAN_DEBUG=0>
        $<$<CONFIG:MinSizeRel>:KRYGA_VULKAN_DEBUG=0>
    )
else()
    target_compile_definitions(vulkan_render INTERFACE
        KRYGA_VULKAN_DEBUG=$<BOOL:${KRYGA_VULKAN_DEBUG}>
    )
endif()

add_library(kryga::vulkan_render ALIAS vulkan_render)

# ----------------------------------------------------------------------------
# vulkan_render_impl_game — STATIC, KRG_BUILD_TEST
# ----------------------------------------------------------------------------
# Non-shipping game build: links ImGui for the drop-down console.
# build_tier.h derives KRG_HAS_IMGUI, KRG_HAS_EDITOR, etc. from the tier.
add_library(vulkan_render_impl_game STATIC ${KRG_VULKAN_RENDER_SRC})
target_compile_definitions(vulkan_render_impl_game PUBLIC KRG_BUILD_TEST=1)
target_link_libraries(vulkan_render_impl_game PUBLIC
    kryga::vulkan_render
    kryga::imgui_unofficial
)
target_precompile_headers(vulkan_render_impl_game PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:<vulkan/vulkan.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<imgui.h$<ANGLE-R>>"
)
kryga_ide_path(vulkan_render_impl_game)
add_library(kryga::vulkan_render_impl_game ALIAS vulkan_render_impl_game)

# ----------------------------------------------------------------------------
# vulkan_render_impl_editor — STATIC, KRG_BUILD_DEV
# ----------------------------------------------------------------------------
# Full ImGui integration: font atlas, UI pipeline, ImGui pass, draw_ui.
add_library(vulkan_render_impl_editor STATIC ${KRG_VULKAN_RENDER_SRC})
target_compile_definitions(vulkan_render_impl_editor PUBLIC KRG_BUILD_DEV=1)
target_link_libraries(vulkan_render_impl_editor PUBLIC
    kryga::vulkan_render
    kryga::imgui_unofficial
)
target_precompile_headers(vulkan_render_impl_editor PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:<vulkan/vulkan.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<imgui.h$<ANGLE-R>>"
)
kryga_ide_path(vulkan_render_impl_editor)
add_library(kryga::vulkan_render_impl_editor ALIAS vulkan_render_impl_editor)

add_subdirectory(private/tests)
