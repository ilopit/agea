#!/bin/bash
# Configure script for agea - runs cmake configuration
# Usage: ./tools/configure.sh [options]
#   -v, --verbose    Show full cmake output
#   -c, --clean      Clean build directory before configure
#   -h, --help       Show this help

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$ROOT_DIR/build"

VERBOSE=0
CLEAN=0

while [[ $# -gt 0 ]]; do
    case $1 in
        -v|--verbose) VERBOSE=1; shift ;;
        -c|--clean)   CLEAN=1; shift ;;
        -h|--help)
            head -6 "$0" | tail -5
            exit 0
            ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

is_cmake_build_dir() {
    local dir="$1"
    [[ -f "$dir/CMakeCache.txt" ]] || [[ -d "$dir/CMakeFiles" ]] || [[ -f "$dir/*.sln" ]]
}

if [[ $CLEAN -eq 1 ]]; then
    if [[ -d "$BUILD_DIR" ]]; then
        if is_cmake_build_dir "$BUILD_DIR"; then
            echo "Cleaning build directory..."
            rm -rf "$BUILD_DIR"
        elif [[ -z "$(ls -A "$BUILD_DIR" 2>/dev/null)" ]]; then
            rm -rf "$BUILD_DIR"
        else
            echo "ERROR: $BUILD_DIR exists but doesn't look like a cmake build directory."
            echo "       Contains: $(ls "$BUILD_DIR" | head -5 | tr '\n' ' ')"
            echo "       Refusing to clean. Remove manually if intended."
            exit 1
        fi
    fi
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

if [[ $VERBOSE -eq 1 ]]; then
    cmake -A x64 ..
else
    cmake -A x64 --log-level=ERROR .. || { cmake -A x64 ..; exit 1; }
fi

echo "Configuration complete. Run tools/build.sh to build."
