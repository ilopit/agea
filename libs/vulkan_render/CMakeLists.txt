set(CMAKE_CXX_STANDARD 17)

file(GLOB LIB_SRC          "include/vulkan_render/*.h"              "src/*.cpp")
file(GLOB DATA_LOADERS_SRC "include/vulkan_render/data_loaders/*.h" "src/data_loaders/*.cpp")

add_library(vulkan_render STATIC
   ${LIB_SRC}
   ${DATA_LOADERS_SRC}
)
target_include_directories(utils PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
add_library(agea::vulkan_render ALIAS vulkan_render)

target_link_libraries(vulkan_render PUBLIC
    agea::utils
    agea::native
    agea::vulkan_render_types
    agea::glm_unofficial
    agea::imgui_unofficial
    agea::stb_unofficial
    agea::resource_locator

    tinyobjloader
    sdl2
)

get_filename_component(PARENT_DIR_1 ${PROJECT_SOURCE_DIR} DIRECTORY)
get_filename_component(PARENT_DIR_2 ${CMAKE_CURRENT_LIST_DIR} DIRECTORY)
file(RELATIVE_PATH rel ${PARENT_DIR_1} ${PARENT_DIR_2})
set_target_properties(vulkan_render PROPERTIES FOLDER ${rel})