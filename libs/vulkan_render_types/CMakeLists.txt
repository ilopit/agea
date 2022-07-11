set(CMAKE_CXX_STANDARD 17)

file(GLOB LIB_SRC "include/vulkan_render_types/*.h" "src/*.cpp")

add_library(vulkan_render_types STATIC
   ${LIB_SRC}
   ${DATA_LOADERS_SRC}
)
target_include_directories(utils PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
add_library(agea::vulkan_render_types ALIAS vulkan_render_types)

target_link_libraries(vulkan_render_types PUBLIC
    vma_unofficial
    vk-bootstrap::vk-bootstrap
    agea::utils
    glm_unofficial
    spirv-reflect-static
)

get_filename_component(PARENT_DIR_1 ${PROJECT_SOURCE_DIR} DIRECTORY)
get_filename_component(PARENT_DIR_2 ${CMAKE_CURRENT_LIST_DIR} DIRECTORY)
file(RELATIVE_PATH rel ${PARENT_DIR_1} ${PARENT_DIR_2})
set_target_properties(vulkan_render_types PROPERTIES FOLDER ${rel})