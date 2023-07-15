##### Reflection
add_custom_target(root.module.ar ALL)
agea_ar_target(root.module.ar root "agea")


##### Model
file(GLOB ROOT_ASSETS_SRC                "public/include/root/assets/*.h"                     "private/src/assets/*.cpp")
file(GLOB ROOT_CACHES_SRC                "public/include/root/caches/*.h"                     "private/src/caches/*.cpp")
file(GLOB ROOT_DEMO_SRC                  "public/include/root/demo/*.h"                       "private/src/demo/*.cpp")
file(GLOB ROOT_CORE_TYPES_SRC            "public/include/root/core_types/*.h"                 "private/src/core_types/*.cpp")
file(GLOB ROOT_COMPONENTS_SRC            "public/include/root/components/*.h"                 "private/src/components/*.cpp")
file(GLOB ROOT_LIGHTS_SRC                "public/include/root/lights/*.h"                     "private/src/lights/*.cpp")
file(GLOB ROOT_SRC                       "public/include/root/*.h"                            "private/src/*.cpp")
file(GLOB ROOT_LIGHTS_COMPONENTS_SRC     "public/include/root/lights/components/*.h"          "private/src/lights/components/*.cpp")
file(GLOB ROOT_REFLECTION_SRC            "public/include/root/reflection/*.h"                 "private/src/reflection/*.cpp")
file(GLOB ROOT_REFLECTION_HANDLERS_SRC   "public/include/root/reflection/type_handlers/*.h"   "private/src/reflection/type_handlers/*.cpp")

file(GLOB GENERATED_SRC                   "${CMAKE_BINARY_DIR}/agea_generated/root/*.cpp")

source_group("assets" FILES  ${ROOT_ASSETS_SRC})
source_group("caches" FILES  ${ROOT_CACHES_SRC})
source_group("components" FILES  ${ROOT_COMPONENTS_SRC})
source_group("lights/components" FILES  ${ROOT_LIGHTS_COMPONENTS_SRC})
source_group("lights" FILES  ${ROOT_LIGHTS_SRC})
source_group("core_types" FILES  ${ROOT_CORE_TYPES_SRC})
source_group("root" FILES  ${ROOT_SRC})
source_group("reflection" FILES  ${ROOT_REFLECTION_SRC})
source_group("generated" FILES  ${GENERATED_SRC})

add_library(root.module.model STATIC
   ${ROOT_COMPONENTS_SRC}
   ${ROOT_ASSETS_SRC}
   ${ROOT_DEMO_SRC}
   ${ROOT_SRC}
   ${ROOT_CACHES_SRC}
   ${ROOT_CORE_TYPES_SRC}
   ${ROOT_LIGHTS_SRC}
   ${ROOT_LIGHTS_COMPONENTS_SRC}
   ${ROOT_REFLECTION_SRC}        
   ${ROOT_REFLECTION_HANDLERS_SRC}
   
   ${GENERATED_SRC}
)

target_compile_options(root.module.model PRIVATE /bigobj)
target_include_directories(root.module.model PUBLIC ${CMAKE_BINARY_DIR}/agea_generated)

target_link_libraries(root.module.model PUBLIC
   agea::ar
   agea::core
   agea::glm_unofficial
   agea::serialization
   agea::resource_locator
   agea::utils
   
   agea::sol2_unofficial

   lua_static
)

agea_finalize_library(root.module.model)

#####  Render
file(GLOB ROOT_RENDER_SRC "public/include/render/*.h" "private/src/render/*.cpp")

source_group("render" FILES ${ROOT_RENDER_SRC})

add_library(root.module.render STATIC
   ${ROOT_RENDER_SRC}
)

target_link_libraries(root.module.render

   agea::root.module.model
   agea::asset_importer
   agea::render_bridge
)

agea_finalize_library(root.module.render)

set(AGEA_ACTIVE_MODULES_TARGETS ${AGEA_ACTIVE_MODULES_TARGETS} root.module.model root.module.render CACHE INTERNAL "")

#####  Tests
add_subdirectory(private/tests)