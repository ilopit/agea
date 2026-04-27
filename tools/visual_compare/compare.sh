#!/bin/bash
# Compare Filament renders against engine renders
# Usage: ./tools/visual_compare/compare.sh
#   Compares images in build/visual_compare/filament/ vs build/visual_compare/engine/
#   Output diffs go to build/visual_compare/diff/

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$(dirname "$SCRIPT_DIR")")"
FILAMENT_DIR="$ROOT_DIR/build/filament"
FILAMENT_RENDERS="$ROOT_DIR/build/visual_compare/filament"
ENGINE_RENDERS="$ROOT_DIR/build/visual_compare/engine"
DIFF_DIR="$ROOT_DIR/build/visual_compare/diff"

if [[ ! -f "$FILAMENT_DIR/bin/diffimg.exe" ]]; then
    echo "Filament not found. Run: tools/fetch_filament.sh"
    exit 1
fi

if [[ ! -d "$FILAMENT_RENDERS" ]]; then
    echo "No Filament renders found. Run: tools/visual_compare/render_filament.sh"
    exit 1
fi

if [[ ! -d "$ENGINE_RENDERS" ]]; then
    echo "No engine renders found."
    echo "Place engine screenshots in: $ENGINE_RENDERS/"
    echo "Filenames should match Filament renders (e.g., shadow_test.png)"
    exit 1
fi

mkdir -p "$DIFF_DIR"

echo "Comparing renders..."
echo ""

for filament_img in "$FILAMENT_RENDERS"/*.png; do
    name=$(basename "$filament_img")
    engine_img="$ENGINE_RENDERS/$name"
    diff_img="$DIFF_DIR/$name"

    if [[ ! -f "$engine_img" ]]; then
        echo "SKIP: $name (no engine render)"
        continue
    fi

    echo "DIFF: $name"
    "$FILAMENT_DIR/bin/diffimg.exe" "$filament_img" "$engine_img" "$diff_img" 2>&1 || true

    if [[ -f "$diff_img" ]]; then
        echo "  -> $diff_img"
    fi
done

echo ""
echo "Results in: $DIFF_DIR/"
echo "Open the diff images to see pixel differences."
