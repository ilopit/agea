file(GLOB LIB_SRC "public/include/asset_converter/*.h" "private/src/*.cpp")

add_library(asset_converter_lib STATIC
    ${LIB_SRC}
)

target_link_libraries(asset_converter_lib
    PUBLIC
    kryga::gpu_types
    kryga::cgltf
    kryga::stb_unofficial
    tinyobjloader
    yaml-cpp
    kryga::core
    kryga::global_state
    kryga::vfs
    kryga::serialization
    packages.root.model
    packages.base.model
)

target_include_directories(asset_converter_lib
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/public/include")

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/private/include")
    target_include_directories(asset_converter_lib
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/private/include")
endif()

add_library(kryga::asset_converter ALIAS asset_converter_lib)

add_executable(asset_converter "private/src/app/main.cpp")

target_link_libraries(asset_converter
    PRIVATE
        kryga::asset_converter
        kryga::utils
        CLI11::CLI11
)

kryga_finalize_executable(asset_converter)

add_subdirectory(private/tests)
