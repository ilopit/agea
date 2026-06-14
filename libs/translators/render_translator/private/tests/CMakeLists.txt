file(GLOB TEST_SOURCES
    "*.h"
    "*.cpp"
)

if(NOT TEST_SOURCES)
    return()
endif()

source_group("test_sources" FILES ${TEST_SOURCES})

add_executable (render_translator_tests
    ${TEST_SOURCES}
 )

target_link_libraries(render_translator_tests
    kryga::render_translator
    gtest
)

kryga_finalize_executable(render_translator_tests)
