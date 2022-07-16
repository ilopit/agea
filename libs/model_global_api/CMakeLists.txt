set(CMAKE_CXX_STANDARD 17)

file(GLOB LIB_SRC "include/model_global_api/*.h" "src/*.cpp")

add_library(model_global_api STATIC
   ${LIB_SRC}
   ${DATA_LOADERS_SRC}
)

target_include_directories(model_global_api PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
add_library(agea::model_global_api ALIAS model_global_api)

target_link_libraries(model_global_api PUBLIC
    agea::vulkan_render_types
)

get_filename_component(PARENT_DIR_1 ${PROJECT_SOURCE_DIR} DIRECTORY)
get_filename_component(PARENT_DIR_2 ${CMAKE_CURRENT_LIST_DIR} DIRECTORY)
file(RELATIVE_PATH rel ${PARENT_DIR_1} ${PARENT_DIR_2})
set_target_properties(model_global_api PROPERTIES FOLDER ${rel})