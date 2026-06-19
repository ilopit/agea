#!/bin/bash
# Android cross-build script — thin wrapper around CMake presets.
#
# Usage: ./tools/build_android.sh [options] [target]
#   -c, --clean      Remove build_android/<config>/ and reconfigure
#   -r, --release    Release configuration (default: Debug)
#   -v, --verbose    Show full configure/build output
#   -j, --jobs N     Parallel jobs (default: cmake auto)
#   -h, --help       Show this help
#   target           Optional target name (default: kryga_ship).
#                    Android entry libs: kryga_ship (SHIP tier, release) or
#                    kryga_game (GAME tier, console + profiler for on-device dev).
#                    Both emit libmain.so; pick one per build (no reconfigure).
#
# Prerequisites:
#   - ANDROID_NDK_HOME pointing at NDK r26d root (see plans/android_ndk_setup.md)
#   - Ninja on PATH, or the one bundled with the NDK will be used
#   - Python on PATH (argen.py runs at configure time)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

CLEAN=0
CONFIG="Debug"
VERBOSE=0
JOBS=""
TARGET="kryga_ship"

while [[ $# -gt 0 ]]; do
    case $1 in
        -c|--clean)   CLEAN=1; shift ;;
        -r|--release) CONFIG="Release"; shift ;;
        -v|--verbose) VERBOSE=1; shift ;;
        -j|--jobs)    JOBS="$2"; shift 2 ;;
        -h|--help)
            head -18 "$0" | tail -17
            exit 0
            ;;
        -*) echo "Unknown option: $1"; exit 1 ;;
        *)  TARGET="$1"; shift ;;
    esac
done

if [[ -z "$ANDROID_NDK_HOME" ]]; then
    echo "ERROR: ANDROID_NDK_HOME is not set."
    echo "       Install NDK r26d per plans/android_ndk_setup.md and export the var."
    exit 1
fi

if [[ ! -f "$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" ]]; then
    echo "ERROR: ANDROID_NDK_HOME=$ANDROID_NDK_HOME does not look like an NDK root."
    echo "       Expected: \$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake"
    exit 1
fi

# Prefer Ninja on PATH; fall back to NDK's bundled copy, then the Android SDK
# CMake package (installed via Android Studio → SDK Manager → CMake).
EXTRA_CFG_ARGS=()
if ! command -v ninja >/dev/null 2>&1; then
    NINJA_EXE=""
    for cand in \
        "$ANDROID_NDK_HOME/prebuilt/windows-x86_64/bin/ninja.exe" \
        "$ANDROID_NDK_HOME/prebuilt/linux-x86_64/bin/ninja" \
        "$ANDROID_NDK_HOME/prebuilt/darwin-x86_64/bin/ninja"; do
        if [[ -x "$cand" ]]; then
            NINJA_EXE="$cand"
            break
        fi
    done
    # Fall back to Android SDK's bundled CMake (ships Ninja alongside cmake.exe)
    if [[ -z "$NINJA_EXE" && -n "$ANDROID_HOME" && -d "$ANDROID_HOME/cmake" ]]; then
        for sdk_cmake in "$ANDROID_HOME/cmake"/*/bin; do
            if [[ -x "$sdk_cmake/ninja.exe" ]]; then
                NINJA_EXE="$sdk_cmake/ninja.exe"
                break
            elif [[ -x "$sdk_cmake/ninja" ]]; then
                NINJA_EXE="$sdk_cmake/ninja"
                break
            fi
        done
    fi
    if [[ -z "$NINJA_EXE" ]]; then
        echo "ERROR: Ninja not found on PATH, in NDK, or in Android SDK CMake package."
        echo "       Install via Android Studio → SDK Manager → SDK Tools → CMake."
        exit 1
    fi
    EXTRA_CFG_ARGS+=(-DCMAKE_MAKE_PROGRAM="$NINJA_EXE")
fi

PRESET_CONFIG=$(echo "$CONFIG" | tr '[:upper:]' '[:lower:]')
PRESET="android-${PRESET_CONFIG}"
BINARY_DIR="$ROOT_DIR/build_android/${PRESET_CONFIG}"

if [[ $CLEAN -eq 1 && -d "$BINARY_DIR" ]]; then
    echo "Cleaning $BINARY_DIR..."
    rm -rf "$BINARY_DIR"
fi

# Configure once. Presets pin generator, toolchain, build type, and binaryDir.
if [[ ! -f "$BINARY_DIR/CMakeCache.txt" ]]; then
    echo "Configuring preset '$PRESET'..."
    CONFIGURE_CMD=(cmake --preset "$PRESET")
    [[ ${#EXTRA_CFG_ARGS[@]} -gt 0 ]] && CONFIGURE_CMD+=("${EXTRA_CFG_ARGS[@]}")

    if [[ $VERBOSE -eq 1 ]]; then
        "${CONFIGURE_CMD[@]}"
    else
        "${CONFIGURE_CMD[@]}" --log-level=ERROR || { "${CONFIGURE_CMD[@]}"; exit 1; }
    fi
fi

echo "Building preset '$PRESET' target=$TARGET..."
BUILD_CMD=(cmake --build --preset "$PRESET")
[[ -n "$TARGET" ]] && BUILD_CMD+=(--target "$TARGET")
[[ -n "$JOBS" ]]   && BUILD_CMD+=(--parallel "$JOBS")

if [[ $VERBOSE -eq 1 ]]; then
    "${BUILD_CMD[@]}" --verbose
else
    "${BUILD_CMD[@]}"
fi

echo "Android build complete ($CONFIG). Output: $BINARY_DIR/"
