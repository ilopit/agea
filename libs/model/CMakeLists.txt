file(GLOB MODEL_ASSETS_SRC                "include/model/assets/*.h"                     "src/assets/*.cpp")
file(GLOB MODEL_CACHES_SRC                "include/model/caches/*.h"                     "src/caches/*.cpp")
file(GLOB MODEL_DEMO_SRC                  "include/model/demo/*.h"                       "src/demo/*.cpp")
file(GLOB MODEL_CORE_TYPES_SRC            "include/model/core_types/*.h"                 "src/core_types/*.cpp")
file(GLOB MODEL_COMPONENTS_SRC            "include/model/components/*.h"                 "src/components/*.cpp")
file(GLOB MODEL_SRC                       "include/model/*.h"                            "src/*.cpp")
file(GLOB MODEL_REFLECTION_SRC            "include/model/reflection/*.h"                 "src/reflection/*.cpp")
file(GLOB MODEL_REFLECTION_HANDLERS_SRC   "include/model/reflection/type_handlers/*.h"   "src/reflection/type_handlers/*.cpp")

file(GLOB GENERATED_SRC                   "${CMAKE_BINARY_DIR}/agea_generated/model/*.cpp")

source_group("assets" FILES  ${MODEL_ASSETS_SRC})
source_group("caches" FILES  ${MODEL_CACHES_SRC})
source_group("components" FILES  ${MODEL_COMPONENTS_SRC})
source_group("core_types" FILES  ${MODEL_CORE_TYPES_SRC})
source_group("demo" FILES  ${MODEL_DEMO_SRC})
source_group("model" FILES  ${MODEL_SRC})
source_group("reflection" FILES  ${MODEL_REFLECTION_SRC})
source_group("generated" FILES  ${GENERATED_SRC})

add_library(model STATIC
   ${MODEL_COMPONENTS_SRC}         
   ${MODEL_ASSETS_SRC}
   ${MODEL_DEMO_SRC}
   ${MODEL_SRC}
   ${MODEL_CACHES_SRC}
   ${MODEL_CORE_TYPES_SRC}
   ${MODEL_REFLECTION_SRC}        
   ${MODEL_REFLECTION_HANDLERS_SRC}
   
   ${GENERATED_SRC}
)

target_compile_options(model PRIVATE /bigobj)

target_link_libraries(model PUBLIC
   agea::ar
   agea::utils
   agea::glm_unofficial
   agea::serialization
   agea::resource_locator
   agea::root

   lua_static
   agea::sol2_unofficial
)

target_include_directories(model PUBLIC ${CMAKE_BINARY_DIR}/agea_generated)

agea_finalize_library(model)

add_subdirectory(tests)