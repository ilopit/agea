file(GLOB TEST_SOURCES
    "*.h"
    "*.cpp"
)
source_group("test_sources" FILES ${TEST_SOURCES})

add_executable (render_utils_tests
    ${TEST_SOURCES}
 )

target_link_libraries(render_utils_tests
    kryga::render_utils
    gtest
    gtest_main
)

kryga_finalize_executable(render_utils_tests)
