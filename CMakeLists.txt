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

# Emit compile_commands.json for clang-tidy / clangd. No effect under the Visual
# Studio generator (host preset) — use the Ninja `host-tidy` preset to get it.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# ---------------------------------------------------------------------------
# Build axes
# ---------------------------------------------------------------------------
# Form factor (mutually exclusive, one always set):
#   KRG_DESKTOP / KRG_MOBILE — set globally based on the toolchain platform.
#
# Build tier (one stamped per engine/render lib via kryga_set_tier(); see
# cmake/utils.cmake and libs/kryga_port/include/kryga_port/build_tier.h). The
# tier is a PUBLIC compile def on the lib, NOT a global — so all tiers coexist
# in a single configure, selected by which exe target you build:
#   kryga_editor — DEV tier:  editor UI, gizmos, console, profiler, RPC.
#   kryga_game   — GAME tier:  console + profiler, no editor.
#   kryga_ship   — SHIP tier:  everything stripped. Smallest binary, the
#                  shipping artifact.
#
# Mobile builds produce kryga_game and kryga_ship (no editor on Android/iOS) —
# each a libmain.so entry, selected per build type (debug=GAME, release=SHIP).
# Desktop builds produce all three by default.
if(ANDROID)
    add_compile_definitions(KRG_MOBILE=1 KRG_PLATFORM_ANDROID=1)
    set(_krg_build_editor_target OFF)
else()
    add_compile_definitions(KRG_DESKTOP=1)
    if(WIN32)
        add_compile_definitions(KRG_PLATFORM_WIN32=1)
    elseif(APPLE)
        add_compile_definitions(KRG_PLATFORM_APPLE=1)
    elseif(UNIX)
        add_compile_definitions(KRG_PLATFORM_LINUX=1)
    endif()
    set(_krg_build_editor_target ON)
endif()

add_compile_definitions(KRG_ENFORCE_READONLY)

option(KRG_BUILD_EDITOR_TARGET  "Build the kryga_editor executable (desktop only)"                    ${_krg_build_editor_target})

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

# Content cooker CLI — editor-only (not built into shipped game). Game devs
# still need cooked content but get it from a prior editor build sharing the
# same build/cooked/ dir; Android sources its cooked tree from the same place
# (gradle task cookContent depends on kryga_cook.exe being built once on the
# desktop editor side).
if(KRG_BUILD_EDITOR_TARGET)
    add_subdirectory(tools/cook)
    add_subdirectory(tools/audio_import)

    # `cook_resources` custom target: invokes kryga_cook.exe over resources/
    # into build/cooked/. Part of ALL so a normal build produces cooked
    # content; kryga_editor depends on it so the symlinks below resolve to
    # actual files at launch time.
    add_custom_target(cook_resources ALL
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
    add_dependencies(kryga_editor cook_resources)
    # The non-editor desktop exes also need cooked content present — depend on
    # it too when they exist in the same build.
    foreach(_krg_exe kryga_game kryga_ship)
        if(TARGET ${_krg_exe})
            add_dependencies(${_krg_exe} cook_resources)
        endif()
    endforeach()
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
