## model
file(GLOB TEST_SOURCES "model/*.h" "model/*.cpp")
source_group("model" FILES ${TEST_SOURCES})

add_executable (packages.test.model.tests ${TEST_SOURCES} )

target_link_libraries(packages.test.model.tests
    kryga::core
    kryga::packages.test.model
    kryga::packages.base.model
    kryga::packages.root.model
    
    gtest
    testing
)

kryga_finalize_executable(packages.test.model.tests)
