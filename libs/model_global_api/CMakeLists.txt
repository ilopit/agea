set(CMAKE_CXX_STANDARD 17)

file(GLOB LIB_SRC "include/model_global_api/*.h" "src/*.cpp")

add_library(model_global_api STATIC
   ${LIB_SRC}
   ${DATA_LOADERS_SRC}
)

target_link_libraries(model_global_api PUBLIC
    agea::vulkan_render_types
)

agea_finalize_library(model_global_api)