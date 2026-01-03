## model
file(GLOB TEST_SOURCES "model/*.h" "model/*.cpp")
source_group("model" FILES ${TEST_SOURCES})

add_executable (packages.root.model.tests ${TEST_SOURCES} )

target_link_libraries(packages.root.model.tests
    kryga::core
    kryga::packages.root.model
    
    gtest
    testing
)

kryga_finalize_executable(packages.root.model.tests)

## render
file(GLOB RENDER_TEST_SOURCES "render/*.h" "render/*.cpp")
source_group("render" FILES ${RENDER_TEST_SOURCES})

add_executable (packages.root.render.tests ${RENDER_TEST_SOURCES} )

target_link_libraries(packages.root.render.tests
    kryga::core
    kryga::packages.root.model
    kryga::packages.root.render
    gtest
    testing
)

kryga_finalize_executable(packages.root.render.tests)