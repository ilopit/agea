## model
file(GLOB TEST_SOURCES "model/*.h" "model/*.cpp")
source_group("model" FILES ${TEST_SOURCES})

add_executable (packages.test.model.tests ${TEST_SOURCES} )

target_link_libraries(packages.test.model.tests
    agea::core
    agea::packages.test.model
    agea::packages.base.model
    agea::packages.root.model
    
    gtest
    testing
)

agea_finalize_executable(packages.test.model.tests)
