file(GLOB TEST_SOURCES
    "*.h"
    "*.cpp"
)
source_group("test_sources" FILES ${TEST_SOURCES})

add_executable (render_bridge_tests
    ${TEST_SOURCES}
 )

target_link_libraries(render_bridge_tests
    agea::render_bridge
    gtest
)

agea_finalize_executable(render_bridge_tests)