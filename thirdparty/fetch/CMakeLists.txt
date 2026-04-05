include(FetchContent)

# Allow FetchContent_Populate for deps without CMakeLists.txt
# (DOWNLOAD_ONLY is broken in CMake 4.1 with git sources)
cmake_policy(SET CMP0169 OLD)

# ==============================================================================
# Existing FetchContent deps
# ==============================================================================

FetchContent_Declare(
  Boost
  URL https://github.com/boostorg/boost/releases/download/boost-1.89.0/boost-1.89.0-cmake.7z

  DOWNLOAD_EXTRACT_TIMESTAMP ON
)

set(BOOST_INCLUDE_LIBRARIES beast url)
FetchContent_MakeAvailable(Boost)

# Tracy profiler
option(TRACY_ENABLE "Enable Tracy profiler" ON)

FetchContent_Declare(
  tracy
  GIT_REPOSITORY https://github.com/wolfpld/tracy.git
  GIT_TAG v0.11.1
  GIT_SHALLOW TRUE
)

set(TRACY_ON_DEMAND ON CACHE BOOL "Enable on-demand profiling")
FetchContent_MakeAvailable(tracy)

# ozz-animation
set(ozz_build_tools ON CACHE BOOL "")
set(ozz_build_samples OFF CACHE BOOL "")
set(ozz_build_tests OFF CACHE BOOL "")
set(ozz_build_howtos OFF CACHE BOOL "")
set(ozz_build_fbx OFF CACHE BOOL "")
set(ozz_build_msvc_rt_dll ON CACHE BOOL "")

FetchContent_Declare(
  ozz-animation
  GIT_REPOSITORY https://github.com/guillaumeblanc/ozz-animation.git
  GIT_TAG 0.16.0
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(ozz-animation)

# ==============================================================================
# Category A: Deps with their own CMakeLists.txt
# ==============================================================================

FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG        1.0.1
  GIT_SHALLOW    TRUE
)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.17.0
  GIT_SHALLOW    TRUE
)

FetchContent_Declare(
  jsoncpp
  GIT_REPOSITORY https://github.com/open-source-parsers/jsoncpp.git
  GIT_TAG        1.9.5
  GIT_SHALLOW    TRUE
)

FetchContent_Declare(
  lua
  GIT_REPOSITORY https://github.com/walterschell/Lua.git
  GIT_TAG        504ef66d500fa1fb4f1684b6617b01342eee704a
)

FetchContent_Declare(
  sol2
  GIT_REPOSITORY https://github.com/ThePhD/sol2.git
  GIT_TAG        v3.3.0
  GIT_SHALLOW    TRUE
)

FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        v1.13.0
  GIT_SHALLOW    TRUE
)

FetchContent_Declare(
  spirv-reflect
  GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Reflect.git
  GIT_TAG        vulkan-sdk-1.4.321.0
  GIT_SHALLOW    TRUE
)

FetchContent_Declare(
  tinyobjloader
  GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
  GIT_TAG        v2.0.0rc13
  GIT_SHALLOW    TRUE
)

FetchContent_Declare(
  vk-bootstrap
  GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
  GIT_TAG        v1.4.327
  GIT_SHALLOW    TRUE
)

FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG        aa8d4e4750ec9fe9f8cc680eb90f1b15955c817e
)

# ==============================================================================
# Category B: No CMakeLists.txt (download only, compiled by unofficial wrappers)
# ==============================================================================

FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        60d7fb207eeb46d6363dd4bde10b35991bae0ce7
)

FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG        af1a5bc352164740c1cc1354942b1c6b72eacb8a
)

FetchContent_Declare(
  textedit
  GIT_REPOSITORY https://github.com/santaclose/ImGuiColorTextEdit.git
  GIT_TAG        23fdb8f6f990711f4643511cb9855482cc7d7ce7
)

FetchContent_Declare(
  vma
  GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
  GIT_TAG        v3.1.0
  GIT_SHALLOW    TRUE
)

FetchContent_Declare(
  cgltf
  GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
  GIT_TAG        v1.15
  GIT_SHALLOW    TRUE
)

# ==============================================================================
# Category C: Patched deps (download only)
# ==============================================================================

FetchContent_Declare(
  imguizmo
  GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
  GIT_TAG        a15acd87a3f3241a29ea1363ceafc680dca3a96b
)

# ==============================================================================
# Make available: Category A (have CMakeLists.txt)
# ==============================================================================

FetchContent_MakeAvailable(
  glm googletest jsoncpp lua sol2 spdlog
  spirv-reflect tinyobjloader vk-bootstrap yaml-cpp
)

# ==============================================================================
# Populate only: Category B + C (no CMakeLists.txt, compiled by unofficial wrappers)
# ==============================================================================

FetchContent_Populate(imgui)
FetchContent_Populate(stb)
FetchContent_Populate(textedit)
FetchContent_Populate(vma)
FetchContent_Populate(cgltf)
FetchContent_Populate(imguizmo)
