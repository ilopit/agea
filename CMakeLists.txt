cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

cmake_policy(VERSION 3.13)


project ("agea")
# Set a default build type if none was specified
include(${PROJECT_SOURCE_DIR}/cmake/utils.cmake)

set(default_build_type "Debug")
if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
  set(default_build_type "Debug")
endif()

if(NOT CMAKE_BUILD_TYPE)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose the type of build." FORCE)
endif()

#set up cmake
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(BUILD_SHARED_LIBS OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

find_package(Vulkan REQUIRED)

add_subdirectory(thirdparty)
add_subdirectory(engine)
add_subdirectory(libs)

execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${PROJECT_SOURCE_DIR}/resources/assets
    ${CMAKE_BINARY_DIR}/project_${CMAKE_BUILD_TYPE}/assets)

execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${PROJECT_SOURCE_DIR}/resources/shaders
    ${CMAKE_BINARY_DIR}/project_${CMAKE_BUILD_TYPE}/shaders)

execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${PROJECT_SOURCE_DIR}/resources/configs
    ${CMAKE_BINARY_DIR}/project_${CMAKE_BUILD_TYPE}/configs)

execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${PROJECT_SOURCE_DIR}/resources/fonts
    ${CMAKE_BINARY_DIR}/project_${CMAKE_BUILD_TYPE}/fonts)

execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${PROJECT_SOURCE_DIR}/resources/levels
    ${CMAKE_BINARY_DIR}/project_${CMAKE_BUILD_TYPE}/levels)

execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${PROJECT_SOURCE_DIR}/resources/objects
    ${CMAKE_BINARY_DIR}/project_${CMAKE_BUILD_TYPE}/objects)

execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${PROJECT_SOURCE_DIR}/resources/components
    ${CMAKE_BINARY_DIR}/project_${CMAKE_BUILD_TYPE}/components)

execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${PROJECT_SOURCE_DIR}/resources/components
    ${CMAKE_BINARY_DIR}/project_${CMAKE_BUILD_TYPE}/components)

execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${PROJECT_SOURCE_DIR}/resources/packages
    ${CMAKE_BINARY_DIR}/project_${CMAKE_BUILD_TYPE}/packages)

execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${PROJECT_SOURCE_DIR}/resources/test
    ${CMAKE_BINARY_DIR}/project_${CMAKE_BUILD_TYPE}/test)