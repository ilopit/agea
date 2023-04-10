file(GLOB TEST_SOURCES
    "*.h"
    "*.cpp"
)
source_group("test_sources" FILES ${TEST_SOURCES})

add_executable (root_tests
    ${TEST_SOURCES}
 )

target_link_libraries(root_tests
    agea::model
    agea::root
    
    gtest
)

agea_finalize_executable(root_tests)