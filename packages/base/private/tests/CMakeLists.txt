## model
file(GLOB TEST_SOURCES "model/*.h" "model/*.cpp")
source_group("model" FILES ${TEST_SOURCES})

add_executable (packages.base.model.tests ${TEST_SOURCES} )

target_link_libraries(packages.base.model.tests
    agea::core
    agea::packages.base.model
    
    gtest
    testing
)

agea_finalize_executable(packages.base.model.tests)

## render
file(GLOB RENDER_TEST_SOURCES "render/*.h" "render/*.cpp")
source_group("render" FILES ${RENDER_TEST_SOURCES})

add_executable (packages.base.render.tests ${RENDER_TEST_SOURCES} )

target_link_libraries(packages.base.render.tests
    agea::core
    agea::packages.base.model
    agea::packages.base.render
    gtest
    testing
)

agea_finalize_executable(packages.base.render.tests)