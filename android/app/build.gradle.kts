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
        minSdk = 29        // Android 10 — lowest device we intend to support
        targetSdk = 34     // Android 14 — matches emulator image we test on (gfxstream Vulkan)
        versionCode = 1
        versionName = "0.1"

        ndk {
            abiFilters += listOf("arm64-v8a", "x86_64")
        }

        externalNativeBuild {
            cmake {
                arguments += listOf(
                    "-DCMAKE_TOOLCHAIN_FILE=${krygaRoot}/cmake/android.toolchain.cmake",
                    "-DBUILD_SHARED_LIBS=OFF",
                    // Echo ANDROID_STL explicitly so AGP auto-packages libc++_shared.so
                    // into the APK. The toolchain sets it internally, but AGP only
                    // reads the top-level CMake arguments here.
                    "-DANDROID_STL=c++_shared"
                )
                // SDL2 is the support lib pulled in as a dep for every variant.
                // The SDL-loaded entry (output named libmain.so) is the game/ship
                // entry lib, selected per build type below (debug=GAME for
                // on-device dev tools, release=SHIP). Both build SHARED with
                // OUTPUT_NAME=main in separate dirs (see engine/CMakeLists.txt).
                // C++ standard is set in root CMakeLists.txt via
                // CMAKE_CXX_STANDARD 23, which clang 17 translates to -std=c++2b
                // (the raw -std=c++23 spelling isn't accepted by NDK r26d's clang).
                targets += listOf("SDL2")
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
            // Cooked content is what the engine reads on-device. The cooker
            // (tools/cook, C++) emits to build/cooked/ with the full runtime
            // tree: precompiled .spv shaders, rewritten shader-effect .aobj
            // descriptors (is_*_binary=true), and every other asset as-is.
            assets.srcDirs("${krygaRoot}/build/cooked")
        }
    }

    buildTypes {
        getByName("release") {
            isMinifyEnabled = false
            // Release APK ships the SHIP tier (everything stripped).
            externalNativeBuild { cmake { targets += listOf("kryga_ship") } }
        }
        getByName("debug") {
            isJniDebuggable = true
            // Debug APK runs the GAME tier (console + profiler on device).
            externalNativeBuild { cmake { targets += listOf("kryga_game") } }
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

// Invoke the C++ cooker (build/project_Debug/bin/kryga_cook.exe) to
// regenerate build/cooked/ before gradle merges assets.
//
// Pre-req: a desktop editor build must have produced kryga_cook.exe at least
// once (the cooker is editor-only — see KRG_BUILD_EDITOR_TARGET in root
// CMakeLists.txt). Same bootstrap requirement as SDL's Java sources under
// build_android/debug/_deps/…. If kryga_cook.exe is missing the task fails
// with a clear message pointing at `tools/build.sh kryga_cook`.
val krygaCookExe = File("${krygaRoot}/build/project_Debug/bin/kryga_cook.exe")

val cookContent = tasks.register<Exec>("cookContent") {
    group = "kryga"
    description = "Runs kryga_cook to produce build/cooked/ for APK packaging"
    workingDir = krygaRoot
    commandLine(
        krygaCookExe.absolutePath,
        "--source", "${krygaRoot}/resources",
        "--output", "${krygaRoot}/build/cooked",
        "--include", "${krygaRoot}/libs/render/gpu_types/public/include",
        "--include", "${krygaRoot}/build/kryga_generated"
    )
    doFirst {
        if (!krygaCookExe.exists()) {
            throw GradleException(
                "kryga_cook.exe not found at ${krygaCookExe.absolutePath}. " +
                "Run `tools/build.sh kryga_cook` on the host first."
            )
        }
    }
    inputs.dir("${krygaRoot}/resources")
    inputs.dir("${krygaRoot}/libs/render/gpu_types/public/include")
    inputs.dir("${krygaRoot}/build/kryga_generated")
    inputs.file(krygaCookExe)
    outputs.dir("${krygaRoot}/build/cooked")
}

androidComponents.onVariants { variant ->
    afterEvaluate {
        tasks.named("merge${variant.name.replaceFirstChar { it.uppercase() }}Assets") {
            dependsOn(cookContent)
        }
    }
}
