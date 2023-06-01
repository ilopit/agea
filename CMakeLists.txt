cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)

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
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(BUILD_SHARED_LIBS OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

find_package(Vulkan REQUIRED)

add_subdirectory(thirdparty)

set(ar_folder ${CMAKE_BINARY_DIR}/agea_generated/engine)
set(modules_file ${ar_folder}/active_modules.cpp)

if(EXISTS ${modules_file})
    message("File already exists, skipping")
else()
    file(MAKE_DIRECTORY ${ar_folder})
    write_file(${ar_folder}/active_modules.h "")
    write_file(${modules_file} "#include \"engine/active_modules.h\"")
endif()

set(AGEA_ACTIVE_MODULES_TARGETS ""  CACHE INTERNAL "")
set(AGEA_ACTIVE_MODULES ""  CACHE INTERNAL "")

add_subdirectory(libs)
add_subdirectory(modules)

add_subdirectory(engine)

execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${PROJECT_SOURCE_DIR}/resources/assets
    ${CMAKE_BINARY_DIR}/project_${CMAKE_BUILD_TYPE}/assets)

execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${PROJECT_SOURCE_DIR}/resources/shaders_includes
    ${CMAKE_BINARY_DIR}/project_${CMAKE_BUILD_TYPE}/shaders_includes)

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

execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${PROJECT_SOURCE_DIR}/resources/tools
    ${CMAKE_BINARY_DIR}/project_${CMAKE_BUILD_TYPE}/tools)