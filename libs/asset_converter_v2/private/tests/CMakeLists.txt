file(GLOB TEST_SOURCES
    "*.h"
    "*.cpp"
)
source_group("test_sources" FILES ${TEST_SOURCES})

add_executable(asset_converter_tests
    ${TEST_SOURCES}
)

target_link_libraries(asset_converter_tests
    gtest
    gmock
    kryga::asset_converter
)

kryga_finalize_executable(asset_converter_tests)
