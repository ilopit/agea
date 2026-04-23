# cmake-js entry point for the Kryga editor native addon.
#
# Build from editor/vscode/native/: `npm install` runs `cmake-js compile`
# which invokes this file with CMAKE_JS_INC / CMAKE_JS_LIB set by cmake-js.

cmake_minimum_required(VERSION 3.15)
project(kryga_native LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# cmake-js passes the node-api include paths and the Windows import lib in
# these variables; on POSIX CMAKE_JS_LIB is empty.
include_directories(${CMAKE_JS_INC})

add_library(kryga_native SHARED
    src/addon.cpp
    # Reuse the engine-side shared-memory wrapper verbatim. The engine lib
    # itself would drag in Vulkan; this one .cpp has no such deps.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../libs/editor_ipc/private/src/shared_memory.cpp
)

# Node addons must be named .node regardless of platform.
set_target_properties(kryga_native PROPERTIES PREFIX "" SUFFIX ".node")

target_link_libraries(kryga_native PRIVATE ${CMAKE_JS_LIB})

if(UNIX AND NOT APPLE)
    target_link_libraries(kryga_native PRIVATE rt)
endif()

# Expose the same header used by the engine-side publisher. Both sides must
# agree on the layout bit-for-bit.
target_include_directories(kryga_native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../libs/editor_ipc/public/include
)
