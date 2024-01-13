file(GLOB TEST_SOURCES
    "*.h"
    "*.cpp"
)
source_group("test_sources" FILES ${TEST_SOURCES})

add_executable (packages.root.tests
    ${TEST_SOURCES}
 )

target_link_libraries(packages.root.tests
    agea::core
    agea::packages.root.types
    
    gtest
)

agea_finalize_executable(packages.root.tests)