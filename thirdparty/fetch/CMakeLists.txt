include(FetchContent)

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
