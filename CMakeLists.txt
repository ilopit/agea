cmake_minimum_required(VERSION 4.0 FATAL_ERROR)
cmake_policy(VERSION 4.0)

project ("kryga")
include(${PROJECT_SOURCE_DIR}/cmake/utils.cmake)

# Set a default build type if none was specified
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)
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
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(BUILD_SHARED_LIBS OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Platform options — Android disables editor/dev-only features
if(ANDROID)
    set(_krg_default_sync_service OFF)
    set(_krg_default_editor OFF)
else()
    set(_krg_default_sync_service ON)
    set(_krg_default_editor ON)
endif()

option(KRG_ENABLE_SYNC_SERVICE "HTTP sync service for live asset/shader reload (requires Boost.Beast)" ${_krg_default_sync_service})
option(KRG_ENABLE_EDITOR "Editor and dev tooling" ${_krg_default_editor})

# On Android, NDK r26d ships Vulkan 1.3.x headers but vk-bootstrap v1.4.x
# references 1.4.x-only types (e.g. VkPhysicalDeviceGlobalPriorityQueryFeatures).
# Fetch Khronos headers matching vk-bootstrap's tag and point Vulkan's include
# dir at them before find_package runs.
if(ANDROID)
    include(FetchContent)
    FetchContent_Declare(
      vulkan_headers
      GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
      GIT_TAG        v1.4.327
      GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(vulkan_headers)
    set(Vulkan_INCLUDE_DIR  "${vulkan_headers_SOURCE_DIR}/include" CACHE PATH "" FORCE)
    set(Vulkan_INCLUDE_DIRS "${vulkan_headers_SOURCE_DIR}/include" CACHE PATH "" FORCE)
endif()

find_package(Vulkan REQUIRED)

add_subdirectory(thirdparty)

set(KRG_ACTIVE_MODULES_TARGETS ""  CACHE INTERNAL "")
set(KRG_ACTIVE_MODULES_TARGETS_LINK ""  CACHE INTERNAL "")
set(KRG_ACTIVE_MODULES ""  CACHE INTERNAL "")

SET(kryga_gendir ${CMAKE_BINARY_DIR}/kryga_generated/engine)

file(MAKE_DIRECTORY ${kryga_gendir})

execute_process(
    COMMAND python
    ${PROJECT_SOURCE_DIR}/tools/argen.py
    --type bind
    --source ${PROJECT_SOURCE_DIR}/packages
    --output ${CMAKE_BINARY_DIR}/kryga_generated)

# Content cook output — shared sink for desktop and Android. The cooker
# (libs/cook + tools/cook) emits:
#   - .vert/.frag/.comp → .spv next to the original path
#   - shader-effect .aobj rewritten to point at .spv + is_*_binary=true
#   - everything else copied
# Desktop runs the engine against this tree; Android bundles it into the APK.
set(KRG_COOKED_DIR ${CMAKE_SOURCE_DIR}/build/cooked)

add_subdirectory(libs)
add_subdirectory(packages)
add_subdirectory(engine)

# Content cooker CLI — desktop only.
if(NOT ANDROID)
    add_subdirectory(tools/cook)

    # `run_cook` custom target: invokes kryga_cook.exe over resources/ into
    # build/cooked/. Part of ALL so a normal build produces cooked content;
    # engine_app depends on it so the symlinks below resolve to actual files
    # at launch time. (Named with `run_` prefix to avoid colliding with the
    # `cook` static library at libs/cook.)
    add_custom_target(run_cook ALL
        COMMAND $<TARGET_FILE:kryga_cook>
                --source ${PROJECT_SOURCE_DIR}/resources
                --output ${KRG_COOKED_DIR}
                --include ${PROJECT_SOURCE_DIR}/libs/render/gpu_types/public/include
                --include ${CMAKE_BINARY_DIR}/kryga_generated
        DEPENDS kryga_cook
        # Cook runs from the build dir so its kryga_log.txt lands inside
        # build/ (gitignored) instead of polluting the source root.
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Cooking resources → ${KRG_COOKED_DIR}"
        VERBATIM)
    add_dependencies(engine_app run_cook)
endif()

# Host-only resource symlinks for local dev runs. On Android, cooked content
# is bundled into the APK via gradle (see android/app/build.gradle.kts) so no
# symlinks are needed.
#
# All content symlinks point at `build/cooked/` rather than `resources/` —
# that's what the engine reads at runtime. `gpu_types` remains a source-tree
# symlink (it's a build-time shader-include root, never consumed by the engine).
# `test_references` also stays source-backed (test harness, not runtime data).
if(NOT ANDROID)
    set(_krg_bin_dir ${CMAKE_BINARY_DIR}/project_${CMAKE_BUILD_TYPE})

    foreach(_sub assets editor shaders_includes shaders configs fonts levels
                 objects components packages test tools)
        execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
            ${KRG_COOKED_DIR}/${_sub}
            ${_krg_bin_dir}/${_sub})
    endforeach()

    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${PROJECT_SOURCE_DIR}/libs/render/gpu_types/public/include
        ${_krg_bin_dir}/gpu_types)

    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${PROJECT_SOURCE_DIR}/resources/test_references
        ${_krg_bin_dir}/test_references)
endif()