set(CMAKE_CXX_STANDARD 17)

file(GLOB LIB_SRC "include/serialization/*.h" "src/*.cpp")

add_library(serialization STATIC
   ${LIB_SRC}
)
target_include_directories(serialization PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
add_library(agea::serialization ALIAS serialization)

target_link_libraries(serialization utils yaml-cpp)

get_filename_component(PARENT_DIR_1 ${PROJECT_SOURCE_DIR} DIRECTORY)
get_filename_component(PARENT_DIR_2 ${CMAKE_CURRENT_LIST_DIR} DIRECTORY)
file(RELATIVE_PATH rel ${PARENT_DIR_1} ${PARENT_DIR_2})
set_target_properties(serialization PROPERTIES FOLDER ${rel})