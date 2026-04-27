#!/bin/bash
# Download Filament prebuilt binaries for visual comparison tests
# Usage: ./tools/fetch_filament.sh [--force]
#   --force    Re-download even if already present

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
FILAMENT_DIR="$ROOT_DIR/build/filament"
FILAMENT_VERSION="v1.70.1"
FILAMENT_URL="https://github.com/google/filament/releases/download/${FILAMENT_VERSION}/filament-${FILAMENT_VERSION}-windows.tgz"

FORCE=0
if [[ "$1" == "--force" ]]; then
    FORCE=1
fi

if [[ -f "$FILAMENT_DIR/bin/gltf_viewer.exe" ]] && [[ $FORCE -eq 0 ]]; then
    echo "Filament already present at $FILAMENT_DIR"
    echo "Use --force to re-download."
    exit 0
fi

echo "Downloading Filament ${FILAMENT_VERSION}..."
mkdir -p "$FILAMENT_DIR"

TMP_FILE="$FILAMENT_DIR/filament.tgz"
curl -L -o "$TMP_FILE" "$FILAMENT_URL"

echo "Extracting..."
tar xzf "$TMP_FILE" -C "$FILAMENT_DIR"
rm "$TMP_FILE"

echo "Filament ${FILAMENT_VERSION} ready at $FILAMENT_DIR"
echo "  gltf_viewer: $FILAMENT_DIR/bin/gltf_viewer.exe"
echo "  diffimg:     $FILAMENT_DIR/bin/diffimg.exe"
