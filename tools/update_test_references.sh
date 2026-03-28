#!/bin/bash
# Regenerate visual regression test reference images.
# Builds and runs vulkan_render_tests with UPDATE_REFERENCES=1.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

CONFIG="Debug"
if [[ "$1" == "-r" || "$1" == "--release" ]]; then
    CONFIG="Release"
fi

BIN_DIR="$ROOT_DIR/build/project_$CONFIG/bin"

echo "Building vulkan_render_tests ($CONFIG)..."
"$SCRIPT_DIR/build.sh" ${CONFIG:+-$([ "$CONFIG" = "Release" ] && echo r)} vulkan_render_tests

echo "Updating reference images..."
cd "$BIN_DIR"
UPDATE_REFERENCES=1 ./vulkan_render_tests.exe --gtest_filter="visual_regression_test.*"

echo "Done. Check resources/test_references/ for updated images."
