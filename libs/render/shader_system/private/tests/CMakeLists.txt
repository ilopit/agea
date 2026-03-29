file(GLOB TEST_SOURCES
    "*.h"
    "*.cpp"
)
source_group("test_sources" FILES ${TEST_SOURCES})

add_executable (shader_system_tests
    ${TEST_SOURCES}
 )

target_link_libraries(shader_system_tests
    gtest
    kryga::shader_system
    kryga::global_state
    kryga::vfs
)

kryga_finalize_executable(shader_system_tests)
