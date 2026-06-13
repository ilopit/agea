file(GLOB TEST_SOURCES
    "*.h"
    "*.cpp"
)
source_group("test_sources" FILES ${TEST_SOURCES})

add_executable(render_types_tests
    ${TEST_SOURCES}
)

target_link_libraries(render_types_tests
    kryga::render_types
    gtest_main
)

kryga_finalize_executable(render_types_tests)
