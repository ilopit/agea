file(GLOB TEST_SOURCES
    "*.h"
    "*.cpp"
)
source_group("test_sources" FILES ${TEST_SOURCES})

add_executable (utils_tests
    ${TEST_SOURCES}
 )

target_link_libraries(utils_tests
    agea::utils
    gtest_main
)

agea_finalize_executable(utils_tests)