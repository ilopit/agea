file(GLOB TEST_SOURCES
    "*.h"
    "*.cpp"
)
source_group("test_sources" FILES ${TEST_SOURCES})

add_executable (vulkan_render_tests
    ${TEST_SOURCES}
 )

target_link_libraries(vulkan_render_tests
    agea::vulkan_render
    gtest
)

agea_finalize_executable(vulkan_render_tests)