##### Reflection
add_custom_target(packages.base.ar ALL)
kryga_ar_target(packages.base.ar base "kryga")

set(BASE_MODEL_PREFIX "public/include/packages/base/model")

##### Model
kryga_gather_sources(BASE_MODEL_ASSETS_SRC            "assets"            "${BASE_MODEL_PREFIX}/assets/*.h"            "private/src/model/assets/*.cpp")
kryga_gather_sources(BASE_MODEL_COMPONENTS_SRC        "components"        "${BASE_MODEL_PREFIX}/components/*.h"        "private/src/model/components/*.cpp")
kryga_gather_sources(BASE_MODEL_LIGHTS_SRC            "lights"            "${BASE_MODEL_PREFIX}/lights/*.h"            "private/src/model/lights/*.cpp")
kryga_gather_sources(BASE_MODEL_LIGHTS_COMPONENTS_SRC "lights/components" "${BASE_MODEL_PREFIX}/lights/components/*.h" "private/src/model/lights/components/*.cpp")
kryga_gather_sources(BASE_MODEL_SRC                   "base"              "${BASE_MODEL_PREFIX}/*.h"                   "private/src/model/*.cpp")
kryga_gather_sources(BASE_MODEL_GENERATED_SRC         "generated"         "${CMAKE_BINARY_DIR}/kryga_generated/packages/base/private/model/*.cpp")

add_library(packages.base.model STATIC
   ${BASE_MODEL_ASSETS_SRC}
   ${BASE_MODEL_COMPONENTS_SRC}
   ${BASE_MODEL_LIGHTS_SRC}
   ${BASE_MODEL_LIGHTS_COMPONENTS_SRC}
   ${BASE_MODEL_SRC}
   ${BASE_MODEL_GENERATED_SRC}
)

target_compile_options(packages.base.model PRIVATE /bigobj)
target_include_directories(packages.base.model PUBLIC
   ${CMAKE_BINARY_DIR}/kryga_generated/packages/base/public/include
   ${CMAKE_BINARY_DIR}/kryga_generated/packages/glue/public/include)

target_link_libraries(packages.base.model PUBLIC
   kryga::ar
   kryga::core
   kryga::glm_unofficial
   kryga::serialization
   kryga::resource_locator
   kryga::utils

   kryga::sol2_unofficial

   lua_static
)
target_compile_definitions(packages.base.model PUBLIC KRG_build__model)
kryga_finalize_library(packages.base.model)

#####  Render
set(BASE_RENDER_PREFIX "public/include/packages/base/render")

kryga_gather_sources(BASE_RENDER_SRC            "render"     "${BASE_RENDER_PREFIX}/*.h"                   "private/src/render/*.cpp")
kryga_gather_sources(BASE_RENDER_OVERRIDES_SRC  "overrides"  "${BASE_RENDER_PREFIX}/overrides/*.h"         "private/src/render/overrides/*.cpp")
kryga_gather_sources(BASE_RENDER_GENERATED_SRC  "generated"  "${CMAKE_BINARY_DIR}/kryga_generated/packages/base/private/render/*.cpp")

add_library(packages.base.render STATIC
   ${BASE_RENDER_SRC}
   ${BASE_RENDER_OVERRIDES_SRC}
   ${BASE_RENDER_GENERATED_SRC}
)

target_compile_options(packages.base.render PRIVATE /bigobj)
target_include_directories(packages.base.render PRIVATE
   ${CMAKE_BINARY_DIR}/kryga_generated)
target_link_libraries(packages.base.render

   kryga::packages.base.model
   kryga::asset_importer
   kryga::render_bridge
   kryga::gpu_types
   kryga::animation
)
target_compile_definitions(packages.base.render PUBLIC KRG_build__render)
kryga_finalize_library(packages.base.render)

#####  Builder
file(GLOB BASE_BUILDER_SRC "public/include/packages/base/builder/*.h" "private/src/builder/*.cpp")

add_library(packages.base.builder STATIC
   ${BASE_BUILDER_SRC}
)

target_link_libraries(packages.base.builder
   kryga::packages.base.model
   kryga::asset_importer
   kryga::packages.base.render
)

target_compile_definitions(packages.base.render PUBLIC KRG_build__builder)
kryga_finalize_library(packages.base.builder)


set(KRG_ACTIVE_MODULES_TARGETS
   ${KRG_ACTIVE_MODULES_TARGETS}
   packages.base.model
   packages.base.render
   packages.base.builder
   CACHE INTERNAL "")

#####  Tests
add_subdirectory(private/tests)
