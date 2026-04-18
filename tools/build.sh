#!/bin/bash
# Build script for kryga host build — thin wrapper around CMake presets.
# Usage: ./tools/build.sh [options] [target]
#   -a, --all        Build all targets (default: engine_app)
#   -v, --verbose    Show full build output
#   -r, --release    Build Release configuration (default: Debug)
#   -j, --jobs N     Parallel jobs (default: cmake auto)
#   -h, --help       Show this help
#   target           Optional target name (default: engine_app)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$ROOT_DIR/build"

VERBOSE=0
CONFIG="Debug"
JOBS=""
TARGET="engine_app"
ALL=0

while [[ $# -gt 0 ]]; do
    case $1 in
        -a|--all)     ALL=1; TARGET=""; shift ;;
        -v|--verbose) VERBOSE=1; shift ;;
        -r|--release) CONFIG="Release"; shift ;;
        -j|--jobs)    JOBS="$2"; shift 2 ;;
        -h|--help)
            head -9 "$0" | tail -8
            exit 0
            ;;
        -*) echo "Unknown option: $1"; exit 1 ;;
        *)  TARGET="$1"; shift ;;
    esac
done

# Configure on first run. The `host` preset pins generator/arch; after that,
# the build dir is reusable across configs (VS multi-config).
if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    echo "build/ not configured — running configure preset 'host'..."
    cmake --preset host
fi

# Preset naming: host-debug, host-release, host-all-debug (-all only at Debug).
PRESET_CONFIG=$(echo "$CONFIG" | tr '[:upper:]' '[:lower:]')
if [[ $ALL -eq 1 ]]; then
    PRESET="host-all-${PRESET_CONFIG}"
else
    PRESET="host-${PRESET_CONFIG}"
fi

BUILD_CMD=(cmake --build --preset "$PRESET")
[[ -n "$TARGET" && $ALL -eq 0 ]] && BUILD_CMD+=(--target "$TARGET")
[[ -n "$JOBS" ]]                 && BUILD_CMD+=(--parallel "$JOBS")

if [[ $VERBOSE -eq 1 ]]; then
    "${BUILD_CMD[@]}"
else
    "${BUILD_CMD[@]}" -- /verbosity:quiet \
        || { echo "Build failed. Re-running verbose:"; "${BUILD_CMD[@]}"; exit 1; }
fi

echo "Build complete ($CONFIG)."
