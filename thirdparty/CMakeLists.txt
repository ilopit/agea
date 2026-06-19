find_package(Vulkan REQUIRED)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

set(BASH_EXECUTABLE "bash.exe"  CACHE INTERNAL "")

function(externalize)
set(name "")
get_target_property(name ${ARGV0} FOLDER)
message(${ARGV0})

if(name STREQUAL "name-NOTFOUND")
set(name "")
endif()

set_target_properties(${ARGV0} PROPERTIES FOLDER "thirdparty/${ARGV1}/${name}")
message(external/${name})
endfunction()

# ==============================================================================
# Config vars (must be set BEFORE FetchContent_MakeAvailable in fetch/)
# ==============================================================================

set(BUILD_SHARED_LIBS OFF)
set(BUILD_STATIC_LIBS ON)

# gtests
set(gtest_force_shared_crt ON)

# jsoncpp
set(JSONCPP_WITH_TESTS OFF)
set(BUILD_OBJECT_LIBS OFF)

# spdlog
# Android's libc++ (NDK r26d / LLVM 17) ships std::format behind C++23 only;
# spdlog's own files get compiled at -std=c++20 regardless of project defaults,
# which makes SPDLOG_USE_STD_FORMAT fail. Use spdlog's bundled fmt on Android.
if(ANDROID)
    set(SPDLOG_USE_STD_FORMAT OFF)
else()
    set(SPDLOG_USE_STD_FORMAT ON)
endif()

# spirvreflect
set(SPIRV_REFLECT_EXECUTABLE OFF)
set(SPIRV_REFLECT_EXAMPLES OFF)
set(SPIRV_REFLECT_STATIC_LIB ON)

# yaml-cpp
set(YAML_CPP_BUILD_TESTS OFF)
set(YAML_CPP_BUILD_TOOLS OFF)
set(YAML_CPP_INSTALL OFF)

# ==============================================================================
# Fetch all dependencies (declares + makes available)
# ==============================================================================

add_subdirectory(fetch)

# externalize fetch deps
externalize(TracyClient "tracy")
externalize(ozz_base "ozz-animation")
externalize(ozz_animation "ozz-animation")
externalize(ozz_animation_offline "ozz-animation")

# externalize Category A deps
externalize(CLI11 "cli11")
externalize(glm "glm")
externalize(jsoncpp_static "jsoncpp")
externalize(lua_static "lua")
externalize(lua_shared "lua")
externalize(luac "lua")
externalize(spdlog "spdlog")
externalize(spirv-reflect-static "spirvreflect")
externalize(tinyobjloader "tinyobjloader")
externalize(vk-bootstrap "vk-bootstrap")
externalize(gtest "gtests")
externalize(gmock "gtests")
externalize(gtest_main "gtests")
externalize(gmock_main "gtests")
externalize(yaml-cpp "yaml-cpp")

# ==============================================================================
# SDL2 (binary download, stays in unofficial)
# ==============================================================================

add_subdirectory(unofficial/sdl2)
externalize(sdl2 "sdl2")

# ==============================================================================
# Unofficial wrappers (compile sources from FetchContent dirs)
# ==============================================================================

#imgui
add_subdirectory(unofficial/imgui)
externalize(imgui_unofficial "imgui")

#imguizmo
add_subdirectory(unofficial/imguizmo)
externalize(imguizmo_unofficial "imguizmo")

#vma
add_subdirectory(unofficial/vma)
externalize(vma_unofficial "vma")

#stb
add_subdirectory(unofficial/stb)
externalize(stb_unofficial "stb")

#cgltf
add_subdirectory(unofficial/cgltf)
externalize(cgltf_unofficial "cgltf")

#xatlas
add_subdirectory(unofficial/xatlas)
externalize(xatlas_unofficial "xatlas")

#miniaudio
add_subdirectory(unofficial/miniaudio)
externalize(miniaudio_unofficial "miniaudio")

#glm
add_subdirectory(unofficial/glm)
externalize(glm_unofficial "glm")

#lua bindings
add_subdirectory(unofficial/lua_bindings)
