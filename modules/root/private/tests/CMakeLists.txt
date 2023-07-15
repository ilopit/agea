file(GLOB TEST_SOURCES
    "*.h"
    "*.cpp"
)
source_group("test_sources" FILES ${TEST_SOURCES})

add_executable (root.module.tests
    ${TEST_SOURCES}
 )

target_link_libraries(root.module.tests
    agea::core
    agea::root.module.model
    
    gtest
)

agea_finalize_executable(root.module.tests)