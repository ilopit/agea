file(GLOB ROOT_ASSETS_SRC                "include/root/assets/*.h"                     "src/assets/*.cpp")
file(GLOB ROOT_CACHES_SRC                "include/root/caches/*.h"                     "src/caches/*.cpp")
file(GLOB ROOT_DEMO_SRC                  "include/root/demo/*.h"                       "src/demo/*.cpp")
file(GLOB ROOT_CORE_TYPES_SRC            "include/root/core_types/*.h"                 "src/core_types/*.cpp")
file(GLOB ROOT_COMPONENTS_SRC            "include/root/components/*.h"                 "src/components/*.cpp")
file(GLOB ROOT_SRC                       "include/root/*.h"                            "src/*.cpp")
file(GLOB ROOT_REFLECTION_SRC            "include/root/reflection/*.h"                 "src/reflection/*.cpp")
file(GLOB ROOT_REFLECTION_HANDLERS_SRC   "include/root/reflection/type_handlers/*.h"   "src/reflection/type_handlers/*.cpp")

file(GLOB GENERATED_SRC                   "${CMAKE_BINARY_DIR}/agea_generated/root/*.cpp")

source_group("assets" FILES  ${ROOT_ASSETS_SRC})
source_group("caches" FILES  ${ROOT_CACHES_SRC})
source_group("components" FILES  ${ROOT_COMPONENTS_SRC})
source_group("core_types" FILES  ${ROOT_CORE_TYPES_SRC})
source_group("root" FILES  ${ROOT_SRC})
source_group("reflection" FILES  ${ROOT_REFLECTION_SRC})
source_group("generated" FILES  ${GENERATED_SRC})

add_library(root STATIC
   ${ROOT_COMPONENTS_SRC}         
   ${ROOT_ASSETS_SRC}
   ${ROOT_DEMO_SRC}
   ${ROOT_SRC}
   ${ROOT_CACHES_SRC}
   ${ROOT_CORE_TYPES_SRC}
   ${ROOT_REFLECTION_SRC}        
   ${ROOT_REFLECTION_HANDLERS_SRC}
   
   ${GENERATED_SRC}
)

target_compile_options(root PRIVATE /bigobj)

target_link_libraries(root PUBLIC
   agea::ar
   agea::utils
   agea::model
   agea::glm_unofficial
   agea::serialization
   agea::resource_locator

   lua_static
   agea::sol2_unofficial
)

agea_finalize_library(root)

target_include_directories(root PUBLIC ${CMAKE_BINARY_DIR}/agea_generated)

add_custom_target(generate.root.ar ALL)
agea_ar_target(generate.root.ar root "agea" 0)


add_subdirectory(tests)