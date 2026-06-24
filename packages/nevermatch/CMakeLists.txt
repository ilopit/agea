##### Reflection
add_custom_target(packages.nevermatch.ar ALL)
kryga_ar_target(packages.nevermatch.ar nevermatch "kryga")

SET(NM_MODEL_PREFIX "public/include/packages/nevermatch")

##### Types
kryga_gather_sources(NM_SRC "nevermatch" "${NM_MODEL_PREFIX}/*.h" "private/src/*.cpp")
kryga_gather_sources(NM_MODEL_SRC "nevermatch" "${NM_MODEL_PREFIX}/model/*.h" "private/src/model/*.cpp")
kryga_gather_sources(NM_MODEL_GENERATED_SRC "generated" "${CMAKE_BINARY_DIR}/kryga_generated/packages/nevermatch/private/model/*.cpp")

add_library(packages.nevermatch.model STATIC
   ${NM_SRC}
   ${NM_MODEL_SRC}
   ${NM_MODEL_GENERATED_SRC}
)
if(MSVC)
    target_compile_options(packages.nevermatch.model PRIVATE /bigobj)
endif()
target_include_directories(packages.nevermatch.model PUBLIC
   ${CMAKE_BINARY_DIR}/kryga_generated/packages/nevermatch/public/include
   ${CMAKE_BINARY_DIR}/kryga_generated/packages/glue/public/include)

target_link_libraries(packages.nevermatch.model PUBLIC
   kryga::ar
   kryga::core
   kryga::glm_unofficial
   kryga::serialization
   kryga::vfs
   kryga::utils

   kryga::game_session
   kryga::picking
   kryga::packages.root.model
   kryga::packages.root.model
)
target_compile_definitions(packages.nevermatch.model PUBLIC KRG_build__model)
kryga_finalize_library(packages.nevermatch.model)


set(KRG_ACTIVE_MODULES_TARGETS
   ${KRG_ACTIVE_MODULES_TARGETS}
   packages.nevermatch.model
   CACHE INTERNAL "")
