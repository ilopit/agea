file(GLOB TEST_SOURCES
    "*.h"
    "*.cpp"
)
source_group("test_sources" FILES ${TEST_SOURCES})

add_executable (shader_compiler_tests
    ${TEST_SOURCES}
 )

target_link_libraries(shader_compiler_tests
    gtest
)

kryga_finalize_executable(shader_compiler_tests)
