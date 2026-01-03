file(GLOB LIB_SRC          "public/include/assets_importer/*.h"              "private/src/*.cpp")

add_library(asset_importer STATIC
   ${LIB_SRC}
)

target_link_libraries(asset_importer PUBLIC
    kryga::utils
    kryga::serialization
    kryga::stb_unofficial
    kryga::vulkan_render
    kryga::core

    tinyobjloader
)
kryga_finalize_library(asset_importer)