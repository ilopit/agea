file(GLOB TEST_SOURCES
    "*.h"
    "*.cpp"
)
source_group("test_sources" FILES ${TEST_SOURCES})

add_executable(engine_regression_tests
    ${TEST_SOURCES}
)

target_link_libraries(engine_regression_tests
    engine
    kryga::render_utils
    kryga::asset_converter
    gtest
)

kryga_finalize_executable(engine_regression_tests)
