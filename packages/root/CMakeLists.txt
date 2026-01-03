##### Reflection
add_custom_target(packages.root.ar ALL)
kryga_ar_target(packages.root.ar root "kryga")

SET(ROOT_MODEL_PREFIX "public/include/packages/root/model")

##### Types
kryga_gather_sources(ROOT_ASSETS_SRC "assets" "${ROOT_MODEL_PREFIX}/assets/*.h" "private/src/model/assets/*.cpp")
kryga_gather_sources(ROOT_COMPONENTS_SRC "components" "${ROOT_MODEL_PREFIX}/components/*.h" "private/src/model/components/*.cpp")
kryga_gather_sources(ROOT_CORE_TYPES_SRC "core_types" "${ROOT_MODEL_PREFIX}/core_types/*.h" "private/src/model/core_types/*.cpp")
kryga_gather_sources(ROOT_EXT_SRC "ext" "${ROOT_MODEL_PREFIX}/ext/*.h" "private/src/model/ext/*.cpp")
kryga_gather_sources(ROOT_OVERRIDES_SRC "overrides" "${ROOT_MODEL_PREFIX}/overrides/*.h" "private/src/model/overrides/*.cpp")
kryga_gather_sources(ROOT_MODEL_SRC "root" "${ROOT_MODEL_PREFIX}/*.h" "private/src/model/*.cpp")
kryga_gather_sources(ROOT_MODEL_GENERATED_SRC "generated" "${CMAKE_BINARY_DIR}/kryga_generated/packages/root/private/model/*.cpp")

add_library(packages.root.model STATIC
   ${ROOT_ASSETS_SRC}
   ${ROOT_COMPONENTS_SRC}
   ${ROOT_CORE_TYPES_SRC}
   ${ROOT_EXT_SRC}
   ${ROOT_OVERRIDES_SRC}
   ${ROOT_MODEL_SRC}
   ${ROOT_MODEL_GENERATED_SRC}
)

target_compile_options(packages.root.model PRIVATE /bigobj)
target_include_directories(packages.root.model PUBLIC
   ${CMAKE_BINARY_DIR}/kryga_generated/packages/root/public/include
   ${CMAKE_BINARY_DIR}/kryga_generated/packages/glue/public/include)

target_link_libraries(packages.root.model PUBLIC
   kryga::ar
   kryga::core
   kryga::glm_unofficial
   kryga::serialization
   kryga::resource_locator
   kryga::utils

   kryga::sol2_unofficial

   lua_static
)
target_compile_definitions(packages.root.model PUBLIC KRG_build__model)
kryga_finalize_library(packages.root.model)

#####  Render
set(ROOT_RENDER_PREFIX "public/include/packages/root/render")
kryga_gather_sources(ROOT_RENDER_SRC "render" "${ROOT_RENDER_PREFIX}/*.h" "private/src/render/*.cpp")
kryga_gather_sources(ROOT_RENDER_OVERRIDES_SRC "overrides" "${ROOT_RENDER_PREFIX}/overrides/*.h" "private/src/render/overrides/*.cpp")
kryga_gather_sources(ROOT_RENDER_GENERATED_SRC "generated" "${CMAKE_BINARY_DIR}/kryga_generated/packages/root/private/render/*.cpp")

add_library(packages.root.render STATIC
   ${ROOT_RENDER_SRC}
   ${ROOT_RENDER_OVERRIDES_SRC}
   ${ROOT_RENDER_GENERATED_SRC}
)

target_compile_options(packages.root.render PRIVATE /bigobj)
target_link_libraries(packages.root.render

   kryga::packages.root.model
   kryga::asset_importer
   kryga::render_bridge
)
target_compile_definitions(packages.root.render PUBLIC KRG_build__render)
kryga_finalize_library(packages.root.render)

#####  Builder
file(GLOB ROOT_BUILDER_SRC "public/include/packages/root/builder/*.h" "private/src/builder/*.cpp")

add_library(packages.root.builder STATIC
   ${ROOT_BUILDER_SRC}
)

target_link_libraries(packages.root.builder
   kryga::packages.root.model
   kryga::asset_importer
   kryga::packages.root.render
)

target_compile_definitions(packages.root.render PUBLIC KRG_build__builder)
kryga_finalize_library(packages.root.builder)


set(KRG_ACTIVE_MODULES_TARGETS
   ${KRG_ACTIVE_MODULES_TARGETS}
   packages.root.model
   packages.root.render
   packages.root.builder
   CACHE INTERNAL "")

#####  Tests
add_subdirectory(private/tests)
