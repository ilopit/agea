#!/bin/bash
# Build script for agea - runs cmake build
# Usage: ./tools/build.sh [options] [target]
#   -a, --all        Build all targets (default: engine_app)
#   -v, --verbose    Show full build output
#   -r, --release    Build Release configuration (default: Debug)
#   -j, --jobs N     Parallel jobs (default: auto)
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

while [[ $# -gt 0 ]]; do
    case $1 in
        -a|--all)     TARGET=""; shift ;;
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

if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    echo "ERROR: Build directory not configured."
    echo "       Run tools/configure.sh first."
    exit 1
fi

cd "$BUILD_DIR"

BUILD_CMD="cmake --build . --config $CONFIG"
[[ -n "$TARGET" ]] && BUILD_CMD="$BUILD_CMD --target $TARGET"
[[ -n "$JOBS" ]] && BUILD_CMD="$BUILD_CMD --parallel $JOBS"

if [[ $VERBOSE -eq 1 ]]; then
    $BUILD_CMD
else
    $BUILD_CMD -- /verbosity:quiet || { echo "Build failed. Re-running verbose:"; $BUILD_CMD; exit 1; }
fi

echo "Build complete ($CONFIG)."
