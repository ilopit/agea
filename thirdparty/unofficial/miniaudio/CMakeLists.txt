FetchContent_GetProperties(miniaudio)

add_library(miniaudio_unofficial STATIC miniaudio_impl.c)

target_include_directories(miniaudio_unofficial PUBLIC "${miniaudio_SOURCE_DIR}")

# Platform audio backends.
# - Windows: WASAPI/DSound/WinMM resolved at runtime, no extra link libs.
# - Android: AAudio/OpenSL are part of the NDK sysroot.
# - Desktop Linux: miniaudio needs pthread, dl and libm.
if(UNIX AND NOT ANDROID AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(miniaudio_unofficial PUBLIC Threads::Threads ${CMAKE_DL_LIBS} m)
endif()

add_library(kryga::miniaudio ALIAS miniaudio_unofficial)
