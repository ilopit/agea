##### Reflection
add_custom_target(packages.root.ar ALL)
agea_ar_target(packages.root.ar root "agea")

SET(ROOT_MODEL_PREFIX "public/include/packages/root/model")

##### Types
agea_gather_sources(ROOT_ASSETS_SRC "assets" "${ROOT_MODEL_PREFIX}/assets/*.h" "private/src/model/assets/*.cpp")
agea_gather_sources(ROOT_COMPONENTS_SRC "components" "${ROOT_MODEL_PREFIX}/components/*.h" "private/src/model/components/*.cpp")
agea_gather_sources(ROOT_CORE_TYPES_SRC "core_types" "${ROOT_MODEL_PREFIX}/core_types/*.h" "private/src/model/core_types/*.cpp")
agea_gather_sources(ROOT_EXT_SRC "ext" "${ROOT_MODEL_PREFIX}/ext/*.h" "private/src/model/ext/*.cpp")
agea_gather_sources(ROOT_OVERRIDES_SRC "overrides" "${ROOT_MODEL_PREFIX}/overrides/*.h" "private/src/model/overrides/*.cpp")
agea_gather_sources(ROOT_MODEL_SRC "root" "${ROOT_MODEL_PREFIX}/*.h" "private/src/model/*.cpp")
agea_gather_sources(ROOT_MODEL_GENERATED_SRC "generated" "${CMAKE_BINARY_DIR}/agea_generated/packages/root/private/model/*.cpp")

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
   ${CMAKE_BINARY_DIR}/agea_generated/packages/root/public/include
   ${CMAKE_BINARY_DIR}/agea_generated/packages/glue/public/include)

target_link_libraries(packages.root.model PUBLIC
   agea::ar
   agea::core
   agea::glm_unofficial
   agea::serialization
   agea::resource_locator
   agea::utils

   agea::sol2_unofficial

   lua_static
)
target_compile_definitions(packages.root.model PUBLIC AGEA_build__model)
agea_finalize_library(packages.root.model)

#####  Render
set(ROOT_RENDER_PREFIX "public/include/packages/root/render")
agea_gather_sources(ROOT_RENDER_SRC "render" "${ROOT_RENDER_PREFIX}/*.h" "private/src/render/*.cpp")
agea_gather_sources(ROOT_RENDER_OVERRIDES_SRC "overrides" "${ROOT_RENDER_PREFIX}/overrides/*.h" "private/src/render/overrides/*.cpp")
agea_gather_sources(ROOT_RENDER_GENERATED_SRC "generated" "${CMAKE_BINARY_DIR}/agea_generated/packages/root/private/render/*.cpp")

add_library(packages.root.render STATIC
   ${ROOT_RENDER_SRC}
   ${ROOT_RENDER_OVERRIDES_SRC}
   ${ROOT_RENDER_GENERATED_SRC}
)

target_compile_options(packages.root.render PRIVATE /bigobj)
target_link_libraries(packages.root.render

   agea::packages.root.model
   agea::asset_importer
   agea::render_bridge
)
target_compile_definitions(packages.root.render PUBLIC AGEA_build__render)
agea_finalize_library(packages.root.render)

#####  Builder
file(GLOB ROOT_BUILDER_SRC "public/include/packages/root/builder/*.h" "private/src/builder/*.cpp")

add_library(packages.root.builder STATIC
   ${ROOT_BUILDER_SRC}
)

target_link_libraries(packages.root.builder
   agea::packages.root.model
   agea::asset_importer
   agea::packages.root.render
)

target_compile_definitions(packages.root.render PUBLIC AGEA_build__builder)
agea_finalize_library(packages.root.builder)


set(AGEA_ACTIVE_MODULES_TARGETS
   ${AGEA_ACTIVE_MODULES_TARGETS}
   packages.root.model
   packages.root.render
   packages.root.builder
   CACHE INTERNAL "")

#####  Tests
add_subdirectory(private/tests)
