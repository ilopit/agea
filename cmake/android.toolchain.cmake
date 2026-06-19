# Kryga Android toolchain wrapper.
#
# Thin wrapper around the NDK's own android.toolchain.cmake that pins our
# project defaults (ABI, API level, STL) so contributors don't have to pass
# them every time.
#
# Use via:
#   cmake -S . -B build_android \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/android.toolchain.cmake \
#         -GNinja -DCMAKE_BUILD_TYPE=Release
#
# Required: ANDROID_NDK_HOME env var pointing at NDK r26d root,
# e.g. C:/Android/ndk/r26d

if(NOT DEFINED ENV{ANDROID_NDK_HOME})
    message(FATAL_ERROR
        "ANDROID_NDK_HOME is not set. Install NDK r26d and point ANDROID_NDK_HOME "
        "at the NDK root (e.g. C:/Android/ndk/r26d).")
endif()

# Kryga's pinned Android config.
set(ANDROID_ABI             "arm64-v8a"  CACHE STRING "")
set(ANDROID_PLATFORM        "android-29" CACHE STRING "")   # Android 10
set(ANDROID_STL             "c++_shared" CACHE STRING "")
set(ANDROID_CPP_FEATURES    "rtti exceptions" CACHE STRING "")

# Hand off to the NDK's real toolchain file.
set(_krg_ndk_toolchain "$ENV{ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake")
if(NOT EXISTS "${_krg_ndk_toolchain}")
    message(FATAL_ERROR
        "Expected NDK toolchain at ${_krg_ndk_toolchain} but it was not found. "
        "Check that ANDROID_NDK_HOME points at the NDK root (not the parent).")
endif()

include("${_krg_ndk_toolchain}")

# CMake 4.x auto-scans C++23 files for `import` (C++20 modules). NDK r26d
# doesn't ship clang-scan-deps, and we don't use modules — disable globally.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF CACHE BOOL "" FORCE)

# The Android entry (kryga_game / kryga_ship) is built as a SHARED library
# (libmain.so), so every static dependency it pulls in must be compiled as PIC.
# Turn this on globally so all static libs (vk-bootstrap, spdlog, etc.) get -fPIC.
set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "" FORCE)

# Project-level defines visible to all targets.
add_compile_definitions(KRG_PLATFORM_ANDROID=1)
