# Kryga Android

Gradle project that wraps the engine's CMake build into an APK.

## Prerequisites

See `plans/android_ndk_setup.md` for NDK r26d install. Additionally needs:

- Android SDK with `platforms;android-29`, one of `build-tools;36.1.0` or
  `37.0.0`, and `cmake;4.1.2` (all available via Android Studio SDK Manager).
- JDK 17+ (bundled as JetBrains Runtime with Android Studio at
  `<AndroidStudio>/jbr/`, or install standalone).
- Gradle wrapper is already checked in — no system Gradle needed.

Before the first Gradle build, set these env vars (the wrapper picks them up):

    export JAVA_HOME="/c/Program Files/Android/Android Studio/jbr"
    export ANDROID_HOME="/c/Users/<you>/AppData/Local/Android/Sdk"
    export ANDROID_NDK_HOME="$ANDROID_HOME/ndk/26.3.11579264"

## Seed the SDL Java sources

The SDL2 Android support classes (`org.libsdl.app.*`) are pulled in by CMake
FetchContent. Gradle references them from
`../build_android/debug/_deps/sdl2_source-src/android-project/app/src/main/java`,
which is populated by the native CLI build. Run it once:

    tools/build_android.sh

After that, Gradle can find the Java sources on every subsequent build.

## Build

From the `android/` directory:

    ./gradlew assembleDebug          # outputs app/build/outputs/apk/debug/app-debug.apk
    ./gradlew assembleRelease

## Install and run

    adb install -r app/build/outputs/apk/debug/app-debug.apk
    adb shell am start -n com.kryga/.KrygaActivity
    adb logcat -v brief -s SDL,Kryga,vulkan

## Known limitations

- First native configure pulls ~500 MB of dependency sources via FetchContent.
- Only `arm64-v8a` is built. armeabi-v7a / x86 / x86_64 are not supported.
- Editor, sync service, and asset importer are disabled on Android.
