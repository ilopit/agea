##### Reflection
add_custom_target(packages.test.ar ALL)
agea_ar_target(packages.test.ar test "agea")

SET(TEST_MODEL_PREFIX "public/include/packages/test")

##### Types
agea_gather_sources(TEST_SRC "test" "${TEST_MODEL_PREFIX}/*.h" "private/src/*.cpp")
agea_gather_sources(TEST_MODEL_SRC "test" "${TEST_MODEL_PREFIX}/model/*.h" "private/src/model/*.cpp")
agea_gather_sources(TEST_MODEL_GENERATED_SRC "generated" "${CMAKE_BINARY_DIR}/agea_generated/packages/test/private/model/*.cpp")

add_library(packages.test.model STATIC
   ${TEST_SRC}
   ${TEST_MODEL_SRC}
   ${TEST_MODEL_GENERATED_SRC}
)
target_compile_options(packages.test.model PRIVATE /bigobj)
target_include_directories(packages.test.model PUBLIC
   ${CMAKE_BINARY_DIR}/agea_generated/packages/test/public/include
   ${CMAKE_BINARY_DIR}/agea_generated/packages/glue/public/include)

target_link_libraries(packages.test.model PUBLIC
   agea::ar
   agea::core
   agea::glm_unofficial
   agea::serialization
   agea::resource_locator
   agea::utils

   agea::packages.base.model
   agea::packages.root.model

   agea::sol2_unofficial

   lua_static
)
target_compile_definitions(packages.test.model PUBLIC AGEA_build__model)
agea_finalize_library(packages.test.model)


set(AGEA_ACTIVE_MODULES_TARGETS
   ${AGEA_ACTIVE_MODULES_TARGETS}
   packages.test.model
   CACHE INTERNAL "")

#####  Tests
add_subdirectory(private/tests)
