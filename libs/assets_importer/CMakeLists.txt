file(GLOB LIB_SRC          "public/include/assets_importer/*.h"              "private/src/*.cpp")

add_library(asset_importer STATIC
   ${LIB_SRC}
)

target_link_libraries(asset_importer PUBLIC
    agea::utils
    agea::serialization
    agea::stb_unofficial
    agea::vulkan_render
    agea::core

    tinyobjloader
)
agea_finalize_library(asset_importer)