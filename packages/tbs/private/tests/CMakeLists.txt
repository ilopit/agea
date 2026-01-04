## model
file(GLOB TEST_SOURCES "model/*.h" "model/*.cpp")
source_group("model" FILES ${TEST_SOURCES})

add_executable (packages.tbs.model.tests ${TEST_SOURCES} )

target_link_libraries(packages.tbs.model.tests
    kryga::core
    kryga::packages.tbs.model
    kryga::packages.base.model
    kryga::packages.root.model

    gtest
    testing
)

kryga_finalize_executable(packages.tbs.model.tests)
