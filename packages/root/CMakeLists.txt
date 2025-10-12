##### Reflection
add_custom_target(packages.root.ar ALL)
agea_ar_target(packages.root.ar root "agea")

SET(ROOT_MODEL_PREFIX "public/include/packages/root/model")

##### Types
file(GLOB ROOT_ASSETS_SRC "${ROOT_MODEL_PREFIX}/assets/*.h" "private/src/model/assets/*.cpp")
file(GLOB ROOT_CACHES_SRC "${ROOT_MODEL_PREFIX}/caches/*.h" "private/src/model/caches/*.cpp")
file(GLOB ROOT_DEMO_SRC "${ROOT_MODEL_PREFIX}/demo/*.h" "private/src/model/demo/*.cpp")
file(GLOB ROOT_EXT_SRC "${ROOT_MODEL_PREFIX}/ext/*.h" "private/src/model/ext/*.cpp")
file(GLOB ROOT_CORE_TYPES_SRC "${ROOT_MODEL_PREFIX}/core_types/*.h" "private/src/model/core_types/*.cpp")
file(GLOB ROOT_COMPONENTS_SRC "${ROOT_MODEL_PREFIX}/components/*.h" "private/src/model/components/*.cpp")
file(GLOB ROOT_LIGHTS_SRC "${ROOT_MODEL_PREFIX}/lights/*.h" "private/src/model/lights/*.cpp")
file(GLOB ROOT_OVERRIDES "${ROOT_MODEL_PREFIX}/overrides/*.h" "private/src/model/overrides/*.cpp")
file(GLOB ROOT_MODEL_SRC "${ROOT_MODEL_PREFIX}/*.h" "private/src/model/*.cpp")
file(GLOB ROOT_LIGHTS_COMPONENTS_SRC "${ROOT_MODEL_PREFIX}/lights/components/*.h" "private/src/model/lights/components/*.cpp")
file(GLOB ROOT_REFLECTION_SRC "${ROOT_MODEL_PREFIX}/reflection/*.h" "private/src/model/reflection/*.cpp")
file(GLOB ROOT_REFLECTION_HANDLERS_SRC "${ROOT_MODEL_PREFIX}/reflection/type_handlers/*.h" "private/src/model/reflection/type_handlers/*.cpp")

file(GLOB ROOT_SRC "public/include/packages/root/*.h" "private/src/model/*.cpp")

file(GLOB ROOT_MODEL_GENERATED_SRC "${CMAKE_BINARY_DIR}/agea_generated/packages/root/private/model/*.cpp")

source_group("assets" FILES ${ROOT_ASSETS_SRC})
source_group("caches" FILES ${ROOT_CACHES_SRC})
source_group("components" FILES ${ROOT_COMPONENTS_SRC})
source_group("lights/components" FILES ${ROOT_LIGHTS_COMPONENTS_SRC})
source_group("lights" FILES ${ROOT_LIGHTS_SRC})
source_group("core_types" FILES ${ROOT_CORE_TYPES_SRC})
source_group("root" FILES ${ROOT_SRC})
source_group("reflection" FILES ${ROOT_REFLECTION_SRC})
source_group("generated" FILES ${ROOT_GENERATED_SRC})
source_group("ext" FILES ${ROOT_EXT_SRC})
source_group("overrides" FILES ${ROOT_OVERRIDES})

add_library(packages.root.model STATIC
   ${ROOT_COMPONENTS_SRC}
   ${ROOT_ASSETS_SRC}
   ${ROOT_DEMO_SRC}
   ${ROOT_SRC}
   ${ROOT_MODEL_SRC}
   ${ROOT_CACHES_SRC}
   ${ROOT_CORE_TYPES_SRC}
   ${ROOT_OVERRIDES}
   ${ROOT_LIGHTS_SRC}
   ${ROOT_LIGHTS_COMPONENTS_SRC}
   ${ROOT_REFLECTION_SRC}
   ${ROOT_REFLECTION_HANDLERS_SRC}

   ${ROOT_EXT_SRC}

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
file(GLOB ROOT_RENDER_SRC "public/include/packages/root/render/*.h" "private/src/render/*.cpp")
file(GLOB ROOT_RENDER_OVERRIDES_SRC "public/include/packages/root/render/overriders/*.h" "private/src/render/overrides/*.cpp")
file(GLOB ROOT_RENDER_GENERATED_SRC "${CMAKE_BINARY_DIR}/agea_generated/packages/root/private/render/*.cpp")

add_library(packages.root.render STATIC
   ${ROOT_RENDER_GENERATED_SRC}
   ${ROOT_RENDER_SRC}
   ${ROOT_RENDER_OVERRIDES_SRC}
)

source_group("render" FILES ${ROOT_RENDER_SRC})
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
