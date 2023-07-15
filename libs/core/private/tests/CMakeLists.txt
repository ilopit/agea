file(GLOB TEST_SOURCES
    "*.h"
    "*.cpp"
)
source_group("test_sources" FILES ${TEST_SOURCES})

add_executable (model_tests
    ${TEST_SOURCES}
 )

target_link_libraries(model_tests
    agea::core
    gtest
)

agea_finalize_executable(model_tests)