# SDL2 platform-conditional:
#   Windows host : prebuilt binary zip from libsdl-org (fast, stable)
#   Android      : source build via FetchContent (needed for arm64-v8a)
#
# Both paths expose an INTERFACE target named `sdl2` that consumers link against.

if(ANDROID)

    FetchContent_Declare(
      sdl2_source
      GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
      GIT_TAG        release-2.32.10
      GIT_SHALLOW    TRUE
    )

    # SDL2 CMake options for Android build.
    set(SDL_STATIC      OFF CACHE BOOL "" FORCE)
    set(SDL_SHARED      ON  CACHE BOOL "" FORCE)  # Android requires libSDL2.so
    set(SDL_TEST        OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS       OFF CACHE BOOL "" FORCE)
    set(SDL2_DISABLE_INSTALL        ON  CACHE BOOL "" FORCE)
    set(SDL2_DISABLE_UNINSTALL      ON  CACHE BOOL "" FORCE)
    set(SDL_VULKAN      ON  CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(sdl2_source)

    add_library(sdl2 INTERFACE)
    target_link_libraries(sdl2 INTERFACE SDL2)
    # SDL2::SDL2 alias may or may not exist depending on SDL version —
    # link against the raw SDL2 target to be safe.

    # Expose the SDL Android Java sources path to consumers (Gradle needs this).
    FetchContent_GetProperties(sdl2_source SOURCE_DIR sdl2_src_dir)
    set(SDL2_ANDROID_JAVA_DIR "${sdl2_src_dir}/android-project/app/src/main/java" CACHE INTERNAL "")

else()

    FetchContent_Declare(
      sdl2_binaries
      URL      https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-devel-2.32.10-VC.zip
    )

    FetchContent_MakeAvailable(sdl2_binaries)

    FetchContent_GetProperties(sdl2_binaries
      SOURCE_DIR sdl2_DIR
    )

    add_library(sdl2 INTERFACE)

    target_include_directories(sdl2 INTERFACE "${sdl2_DIR}/include")
    target_link_directories(sdl2 INTERFACE "${sdl2_DIR}/lib/x64")
    target_link_libraries(sdl2 INTERFACE SDL2 SDL2main)

    configure_file(
        "${sdl2_DIR}/lib/x64/SDL2.dll"
        "${CMAKE_BINARY_DIR}/project_${CMAKE_BUILD_TYPE}/bin/SDL2.dll" COPYONLY)

endif()
