file(GLOB TEST_SOURCES
    "*.h"
    "*.cpp"
)
source_group("test_sources" FILES ${TEST_SOURCES})

add_executable (render_utils_tests
    ${TEST_SOURCES}
 )

target_link_libraries(render_utils_tests
    agea::render_utils
    gtest
)

agea_finalize_executable(render_utils_tests)
