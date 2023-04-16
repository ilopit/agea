file(GLOB TEST_SOURCES
    "*.h"
    "*.cpp"
)
source_group("test_sources" FILES ${TEST_SOURCES})

add_executable (root.m.tests
    ${TEST_SOURCES}
 )

target_link_libraries(root.m.tests
    agea::core
    agea::root.m.model
    
    gtest
)

agea_finalize_executable(root.m.tests)