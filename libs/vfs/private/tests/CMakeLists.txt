file(GLOB TEST_SOURCES
    "*.h"
    "*.cpp"
)
source_group("test_sources" FILES ${TEST_SOURCES})

add_executable(vfs_tests
    ${TEST_SOURCES}
)

target_link_libraries(vfs_tests
    gtest
    gmock
    kryga::vfs
    kryga::global_state
)

kryga_finalize_executable(vfs_tests)
