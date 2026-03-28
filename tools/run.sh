#!/bin/bash
# Run a built executable from the correct bin directory
# Usage: ./tools/run.sh [-r] <executable> [args...]
#   -r            Use Release build (default: Debug)
#   executable    Name of the .exe to run
#   args          Arguments passed to the executable

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

CONFIG="Debug"
if [[ "$1" == "-r" || "$1" == "--release" ]]; then
    CONFIG="Release"
    shift
fi

if [[ $# -eq 0 ]]; then
    echo "Usage: tools/run.sh [-r] <executable> [args...]"
    echo ""
    echo "Available executables:"
    ls "$ROOT_DIR/build/project_$CONFIG/bin/"*.exe 2>/dev/null | xargs -n1 basename
    exit 1
fi

BIN_DIR="$ROOT_DIR/build/project_$CONFIG/bin"
EXE="$1"
shift

if [[ ! -f "$BIN_DIR/$EXE" ]]; then
    echo "ERROR: $EXE not found in $BIN_DIR"
    echo ""
    echo "Available executables:"
    ls "$BIN_DIR/"*.exe 2>/dev/null | xargs -n1 basename
    exit 1
fi

cd "$BIN_DIR"
./"$EXE" "$@"
