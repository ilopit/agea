##### Reflection
add_custom_target(packages.tbs.ar ALL)
kryga_ar_target(packages.tbs.ar tbs "kryga")

SET(TBS_MODEL_PREFIX "public/include/packages/tbs")

##### Types
kryga_gather_sources(TBS_SRC "tbs" "${TBS_MODEL_PREFIX}/*.h" "private/src/*.cpp")
kryga_gather_sources(TBS_MODEL_SRC "tbs" "${TBS_MODEL_PREFIX}/model/*.h" "private/src/model/*.cpp")
kryga_gather_sources(TBS_MODEL_GENERATED_SRC "generated" "${CMAKE_BINARY_DIR}/kryga_generated/packages/tbs/private/model/*.cpp")

add_library(packages.tbs.model STATIC
   ${TBS_SRC}
   ${TBS_MODEL_SRC}
   ${TBS_MODEL_GENERATED_SRC}
)
target_compile_options(packages.tbs.model PRIVATE /bigobj)
target_include_directories(packages.tbs.model PUBLIC
   ${CMAKE_BINARY_DIR}/kryga_generated/packages/tbs/public/include
   ${CMAKE_BINARY_DIR}/kryga_generated/packages/glue/public/include)

target_link_libraries(packages.tbs.model PUBLIC
   kryga::ar
   kryga::core
   kryga::glm_unofficial
   kryga::serialization

   kryga::vfs
   kryga::utils

   kryga::packages.base.model
   kryga::packages.root.model
)
target_compile_definitions(packages.tbs.model PUBLIC KRG_build__model)
kryga_finalize_library(packages.tbs.model)


set(KRG_ACTIVE_MODULES_TARGETS
   ${KRG_ACTIVE_MODULES_TARGETS}
   packages.tbs.model
   CACHE INTERNAL "")

#####  Tests
add_subdirectory(private/tests)
