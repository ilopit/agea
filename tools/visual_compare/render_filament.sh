#!/bin/bash
# Render test scenes with Filament for visual comparison
# Usage: ./tools/visual_compare/render_filament.sh [scene_name]
#   Launches Filament viewer. Press 'S' to screenshot.
#   Screenshots are saved next to the .glb file, copy to build/visual_compare/filament/

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$(dirname "$SCRIPT_DIR")")"
FILAMENT_DIR="$ROOT_DIR/build/filament"
FILAMENT_BIN="$FILAMENT_DIR/bin"
SCENES_DIR="$SCRIPT_DIR/scenes"
OUTPUT_DIR="$ROOT_DIR/build/visual_compare/filament"

if [[ ! -f "$FILAMENT_BIN/gltf_viewer.exe" ]]; then
    echo "Filament not found. Run: tools/fetch_filament.sh"
    exit 1
fi

# Generate scenes if not present
if [[ ! -d "$SCENES_DIR" ]] || [[ -z "$(ls "$SCENES_DIR"/*.glb 2>/dev/null)" ]]; then
    echo "Generating test scenes..."
    mkdir -p "$SCENES_DIR"
    cd "$SCENES_DIR"
    python "$SCRIPT_DIR/generate_scenes.py"
    cd "$ROOT_DIR"
fi

mkdir -p "$OUTPUT_DIR"

SCENE_NAME="${1:-shadow_test}"
SCENE_FILE="$SCENES_DIR/${SCENE_NAME}.glb"

if [[ ! -f "$SCENE_FILE" ]]; then
    echo "Scene not found: $SCENE_FILE"
    echo "Available scenes:"
    ls "$SCENES_DIR"/*.glb 2>/dev/null | xargs -n1 basename
    exit 1
fi

echo "=== Filament Visual Compare ==="
echo "Scene: $SCENE_NAME"
echo ""
echo "Controls:"
echo "  Mouse drag  - rotate camera"
echo "  Scroll      - zoom"
echo "  S           - save screenshot (saved next to .glb)"
echo "  Light panel - adjust shadows, sun direction"
echo ""
echo "After capturing, copy screenshot to:"
echo "  $OUTPUT_DIR/"
echo ""
echo "Launching..."

cd "$FILAMENT_BIN"
./gltf_viewer.exe --api vulkan --actual-size "$SCENE_FILE"
