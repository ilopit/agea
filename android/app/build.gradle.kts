plugins {
    id("com.android.application")
}

// Kryga project root (one dir up from android/).
val krygaRoot: File = rootDir.parentFile

android {
    namespace = "com.kryga"
    compileSdk = 34
    ndkVersion = "26.3.11579264"  // NDK r26d

    defaultConfig {
        applicationId = "com.kryga"
        minSdk = 29        // Android 10
        targetSdk = 29
        versionCode = 1
        versionName = "0.1"

        ndk {
            abiFilters += listOf("arm64-v8a", "x86_64")
        }

        externalNativeBuild {
            cmake {
                arguments += listOf(
                    "-DCMAKE_TOOLCHAIN_FILE=${krygaRoot}/cmake/android.toolchain.cmake",
                    "-DKRG_ENABLE_SYNC_SERVICE=OFF",
                    "-DKRG_ENABLE_EDITOR=OFF",
                    "-DBUILD_SHARED_LIBS=OFF",
                    // Echo ANDROID_STL explicitly so AGP auto-packages libc++_shared.so
                    // into the APK. The toolchain sets it internally, but AGP only
                    // reads the top-level CMake arguments here.
                    "-DANDROID_STL=c++_shared"
                )
                // libmain.so is the SDL-loaded entry; libSDL2.so is pulled in as a dep.
                // C++ standard is set in root CMakeLists.txt via CMAKE_CXX_STANDARD 23,
                // which clang 17 translates to -std=c++2b (the raw -std=c++23 spelling
                // isn't accepted by NDK r26d's clang).
                targets += listOf("engine_app", "SDL2")
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("${krygaRoot}/CMakeLists.txt")
            version = "4.1.2"
        }
    }

    sourceSets {
        getByName("main") {
            // Kryga's own Java (activity subclass) lives under app/src/main/java.
            // SDL's Java support classes (org.libsdl.app.*) come from the
            // FetchContent-populated SDL source tree. We reuse the CLI build's
            // _deps cache (build_android/debug/_deps) which has a stable path
            // with no CMake-variant hash. Requires `tools/build_android.sh` to
            // have been run once to seed the sources.
            java.srcDirs(
                "src/main/java",
                "${krygaRoot}/build_android/debug/_deps/sdl2_source-src/android-project/app/src/main/java"
            )
            assets.srcDirs("${krygaRoot}/resources")
        }
    }

    buildTypes {
        getByName("release") {
            isMinifyEnabled = false
        }
        getByName("debug") {
            isJniDebuggable = true
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}

dependencies {
    implementation("androidx.appcompat:appcompat:1.6.1")
}
