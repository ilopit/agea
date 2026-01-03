file(GLOB TEST_SOURCES
    "*.h"
    "*.cpp"
)
source_group("test_sources" FILES ${TEST_SOURCES})

add_executable (utils_tests
    ${TEST_SOURCES}
 )

target_link_libraries(utils_tests
    kryga::utils
    gtest_main
)

kryga_finalize_executable(utils_tests)