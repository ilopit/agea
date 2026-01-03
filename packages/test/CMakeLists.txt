##### Reflection
add_custom_target(packages.test.ar ALL)
kryga_ar_target(packages.test.ar test "kryga")

SET(TEST_MODEL_PREFIX "public/include/packages/test")

##### Types
kryga_gather_sources(TEST_SRC "test" "${TEST_MODEL_PREFIX}/*.h" "private/src/*.cpp")
kryga_gather_sources(TEST_MODEL_SRC "test" "${TEST_MODEL_PREFIX}/model/*.h" "private/src/model/*.cpp")
kryga_gather_sources(TEST_MODEL_GENERATED_SRC "generated" "${CMAKE_BINARY_DIR}/kryga_generated/packages/test/private/model/*.cpp")

add_library(packages.test.model STATIC
   ${TEST_SRC}
   ${TEST_MODEL_SRC}
   ${TEST_MODEL_GENERATED_SRC}
)
target_compile_options(packages.test.model PRIVATE /bigobj)
target_include_directories(packages.test.model PUBLIC
   ${CMAKE_BINARY_DIR}/kryga_generated/packages/test/public/include
   ${CMAKE_BINARY_DIR}/kryga_generated/packages/glue/public/include)

target_link_libraries(packages.test.model PUBLIC
   kryga::ar
   kryga::core
   kryga::glm_unofficial
   kryga::serialization
   kryga::resource_locator
   kryga::utils

   kryga::packages.base.model
   kryga::packages.root.model

   kryga::sol2_unofficial

   lua_static
)
target_compile_definitions(packages.test.model PUBLIC KRG_build__model)
kryga_finalize_library(packages.test.model)


set(KRG_ACTIVE_MODULES_TARGETS
   ${KRG_ACTIVE_MODULES_TARGETS}
   packages.test.model
   CACHE INTERNAL "")

#####  Tests
add_subdirectory(private/tests)
