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

# All paths are discovered at runtime via kryga::paths::resolve() — no
# compile-time path defines. Test fails loudly if no kryga.project anchor
# is reachable from argv[0] / CWD or no KRYGA_PROJECT_ROOT env var is set.

kryga_finalize_executable(engine_regression_tests)
