# Android NDK Setup — Kryga

Target: NDK r26d, standalone (no Android Studio), Windows host.

## 1. Download NDK r26d

Direct download (SHA1: `fcd6d6dcea7a4e6bc380c13ffdde2bf07a3db70e`):

    https://dl.google.com/android/repository/android-ndk-r26d-windows.zip

Archive size: ~1.1 GB. Extract to `C:\Android\ndk\`. Final layout:

    C:\Android\ndk\r26d\
        build\
        sources\
        toolchains\
        ...

## 2. Set env vars (PowerShell, persistent)

    [Environment]::SetEnvironmentVariable('ANDROID_NDK_HOME', 'C:\Android\ndk\r26d', 'User')
    [Environment]::SetEnvironmentVariable('ANDROID_NDK_ROOT', 'C:\Android\ndk\r26d', 'User')

Restart the terminal after setting these.

## 3. Verify

    %ANDROID_NDK_HOME%\ndk-build.cmd --version
    # Expect: GNU Make 4.3 (or similar)

    dir %ANDROID_NDK_HOME%\build\cmake\android.toolchain.cmake
    # Must exist — our cmake/android.toolchain.cmake includes it.

## 4. (Optional) Android SDK + platform-tools

Only needed for APK signing, `adb` device install, and Gradle builds. Skip if you only
want to NDK-cross-compile the engine without packaging an APK yet.

Install via `sdkmanager` command-line tools:

    https://developer.android.com/studio#command-line-tools-only

Download `commandlinetools-win-*.zip`, extract to `C:\Android\sdk\cmdline-tools\latest\`.
Then:

    set ANDROID_HOME=C:\Android\sdk
    %ANDROID_HOME%\cmdline-tools\latest\bin\sdkmanager.bat "platform-tools" "platforms;android-29" "build-tools;34.0.0"

Add `%ANDROID_HOME%\platform-tools` to PATH for `adb`.

## 5. First configure

    cmake -S . -B build_android ^
          -DCMAKE_TOOLCHAIN_FILE=cmake/android.toolchain.cmake ^
          -G Ninja ^
          -DCMAKE_BUILD_TYPE=Release

Expected: configure succeeds, `KRG_ENABLE_SYNC_SERVICE` is OFF (skips Boost download),
`KRG_ENABLE_EDITOR` is OFF. SDL2 source is fetched and configured for arm64-v8a.

## 6. First build

    cmake --build build_android --target engine

If the engine static lib links, Phase 1 is done. APK packaging is Phase 1.2 (android/ Gradle project).
