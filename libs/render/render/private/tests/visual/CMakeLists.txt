file(GLOB TEST_SOURCES
    "*.h"
    "*.cpp"
)
source_group("test_sources" FILES ${TEST_SOURCES})

add_executable(visual_regression_tests
    ${TEST_SOURCES}
)

target_link_libraries(visual_regression_tests
    kryga::vulkan_render
    kryga::render_utils
    gtest
)

kryga_finalize_executable(visual_regression_tests)
