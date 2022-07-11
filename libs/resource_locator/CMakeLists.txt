set(CMAKE_CXX_STANDARD 17)

file(GLOB LIB_SRC "include/resource_locator/*.h" "src/*.cpp")

add_library(resource_locator STATIC
   ${LIB_SRC}
)
target_include_directories(resource_locator PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
add_library(agea::resource_locator ALIAS resource_locator)

target_link_libraries(resource_locator 
    spdlog 
    utils
)

get_filename_component(PARENT_DIR_1 ${PROJECT_SOURCE_DIR} DIRECTORY)
get_filename_component(PARENT_DIR_2 ${CMAKE_CURRENT_LIST_DIR} DIRECTORY)
file(RELATIVE_PATH rel ${PARENT_DIR_1} ${PARENT_DIR_2})
set_target_properties(resource_locator PROPERTIES FOLDER ${rel})