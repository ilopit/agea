
file(GLOB CORE_CACHES_SRC                "public/include/core/caches/*.h"                     "private/src/caches/*.cpp")
file(GLOB CORE_SRC                       "public/include/core/*.h"                            "private/src/*.cpp")
file(GLOB CORE_REFLECTION_SRC            "public/include/core/reflection/*.h"                 "private/src/reflection/*.cpp")
file(GLOB CORE_REFLECTION_HANDLERS_SRC   "public/include/core/reflection/type_handlers/*.h"   "private/src/reflection/type_handlers/*.cpp")

file(GLOB GENERATED_SRC                   "${CMAKE_BINARY_DIR}/agea_generated/core/*.cpp")

source_group("caches" FILES  ${CORE_CACHES_SRC})
source_group("core" FILES  ${CORE_SRC})
source_group("reflection" FILES  ${CORE_REFLECTION_SRC})


add_library(core STATIC
   ${CORE_COMPONENTS_SRC}         
   ${CORE_ASSETS_SRC}
   ${CORE_DEMO_SRC}
   ${CORE_SRC}
   ${CORE_CACHES_SRC}
   ${CORE_CORE_TYPES_SRC}
   ${CORE_REFLECTION_SRC}        
   ${CORE_REFLECTION_HANDLERS_SRC}
   
   ${GENERATED_SRC}
)

target_compile_options(core PRIVATE /bigobj)

target_link_libraries(core PUBLIC
   agea::ar

   agea::packages.root.model
   
   agea::glm_unofficial
   agea::error_handling
   agea::resource_locator
   agea::serialization
   agea::utils

   lua_static
   agea::sol2_unofficial
)

agea_finalize_library(core)

add_subdirectory(private/tests)