##### Reflection
add_custom_target(packages.ui.ar ALL)
kryga_ar_target(packages.ui.ar ui "kryga")

set(UI_MODEL_PREFIX "public/include/packages/ui/model")

##### Model
kryga_gather_sources(UI_MODEL_SRC_MODEL "model" "${UI_MODEL_PREFIX}/*.h" "private/src/model/*.cpp")
kryga_gather_sources(UI_MODEL_SRC_ROOT  "ui"    "public/include/packages/ui/*.h" "private/src/*.cpp")
kryga_gather_sources(UI_MODEL_GENERATED_SRC "generated" "${CMAKE_BINARY_DIR}/kryga_generated/packages/ui/private/model/*.cpp")

add_library(packages.ui.model STATIC
   ${UI_MODEL_SRC_MODEL}
   ${UI_MODEL_SRC_ROOT}
   ${UI_MODEL_GENERATED_SRC}
)

target_compile_options(packages.ui.model PRIVATE /bigobj)
target_include_directories(packages.ui.model PUBLIC
   ${CMAKE_BINARY_DIR}/kryga_generated/packages/ui/public/include
   ${CMAKE_BINARY_DIR}/kryga_generated/packages/glue/public/include)

target_link_libraries(packages.ui.model PUBLIC
   kryga::ar
   kryga::core
   kryga::glm_unofficial
   kryga::serialization

   kryga::vfs
   kryga::utils

   kryga::packages.root.model
)
target_compile_definitions(packages.ui.model PUBLIC KRG_build__model)
kryga_finalize_library(packages.ui.model)

##### Render
set(UI_RENDER_PREFIX "public/include/packages/ui/render")

kryga_gather_sources(UI_RENDER_SRC            "render"     "${UI_RENDER_PREFIX}/*.h"             "private/src/render/*.cpp")
kryga_gather_sources(UI_RENDER_OVERRIDES_SRC  "overrides"  "${UI_RENDER_PREFIX}/overrides/*.h"   "private/src/render/overrides/*.cpp")
kryga_gather_sources(UI_RENDER_GENERATED_SRC  "generated"  "${CMAKE_BINARY_DIR}/kryga_generated/packages/ui/private/render/*.cpp")

add_library(packages.ui.render STATIC
   ${UI_RENDER_SRC}
   ${UI_RENDER_OVERRIDES_SRC}
   ${UI_RENDER_GENERATED_SRC}
)

target_compile_options(packages.ui.render PRIVATE /bigobj)
target_include_directories(packages.ui.render PRIVATE
   ${CMAKE_BINARY_DIR}/kryga_generated)
target_link_libraries(packages.ui.render
   kryga::packages.ui.model
   kryga::render_bridge
   kryga::vulkan_render
)
target_compile_definitions(packages.ui.render PUBLIC KRG_build__render)
kryga_finalize_library(packages.ui.render)


set(KRG_ACTIVE_MODULES_TARGETS
   ${KRG_ACTIVE_MODULES_TARGETS}
   packages.ui.model
   packages.ui.render
   CACHE INTERNAL "")
