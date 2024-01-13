##### Reflection
add_custom_target(packages.root.ar ALL)
agea_ar_target(packages.root.ar root "agea")


##### Types
file(GLOB ROOT_ASSETS_SRC                "public/include/packages/root/assets/*.h"                     "private/src/assets/*.cpp")
file(GLOB ROOT_CACHES_SRC                "public/include/packages/root/caches/*.h"                     "private/src/caches/*.cpp")
file(GLOB ROOT_DEMO_SRC                  "public/include/packages/root/demo/*.h"                       "private/src/demo/*.cpp")
file(GLOB ROOT_CORE_TYPES_SRC            "public/include/packages/root/core_types/*.h"                 "private/src/core_types/*.cpp")
file(GLOB ROOT_COMPONENTS_SRC            "public/include/packages/root/components/*.h"                 "private/src/components/*.cpp")
file(GLOB ROOT_LIGHTS_SRC                "public/include/packages/root/lights/*.h"                     "private/src/lights/*.cpp")
file(GLOB ROOT_SRC                       "public/include/packages/root/*.h"                            "private/src/*.cpp")
file(GLOB ROOT_LIGHTS_COMPONENTS_SRC     "public/include/packages/root/lights/components/*.h"          "private/src/lights/components/*.cpp")
file(GLOB ROOT_REFLECTION_SRC            "public/include/packages/root/reflection/*.h"                 "private/src/reflection/*.cpp")
file(GLOB ROOT_REFLECTION_HANDLERS_SRC   "public/include/packages/root/reflection/type_handlers/*.h"   "private/src/reflection/type_handlers/*.cpp")

file(GLOB ROOT_GENERATED_SRC             "${CMAKE_BINARY_DIR}/agea_generated/packages/root/*.cpp")

source_group("assets"            FILES  ${ROOT_ASSETS_SRC})
source_group("caches"            FILES  ${ROOT_CACHES_SRC})
source_group("components"        FILES  ${ROOT_COMPONENTS_SRC})
source_group("lights/components" FILES  ${ROOT_LIGHTS_COMPONENTS_SRC})
source_group("lights"            FILES  ${ROOT_LIGHTS_SRC})
source_group("core_types"        FILES  ${ROOT_CORE_TYPES_SRC})
source_group("root"              FILES  ${ROOT_SRC})
source_group("reflection"        FILES  ${ROOT_REFLECTION_SRC})
source_group("generated"         FILES  ${ROOT_GENERATED_SRC})

add_library(packages.root.types STATIC
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

   ${ROOT_GENERATED_SRC}
)

target_compile_options(packages.root.types PRIVATE /bigobj)
target_include_directories(packages.root.types PUBLIC ${CMAKE_BINARY_DIR}/agea_generated)

target_link_libraries(packages.root.types PUBLIC
   agea::ar
   agea::core
   agea::glm_unofficial
   agea::serialization
   agea::resource_locator
   agea::utils
   
   agea::sol2_unofficial

   lua_static
)

agea_finalize_library(packages.root.types)

#####  Render
file(GLOB ROOT_RENDER_SRC                "public/include/packages/root/render/*.h"                     "private/src/render/*.cpp")

source_group("render" FILES ${ROOT_RENDER_SRC})

add_library(packages.root.render_bridge STATIC
   ${ROOT_RENDER_SRC}
)

target_link_libraries(packages.root.render_bridge

   agea::packages.root.types
   agea::asset_importer
   agea::render_bridge
)

agea_finalize_library(packages.root.render_bridge)

set(AGEA_ACTIVE_MODULES_TARGETS ${AGEA_ACTIVE_MODULES_TARGETS} packages.root.types packages.root.render_bridge CACHE INTERNAL "")

#####  Tests
add_subdirectory(private/tests)