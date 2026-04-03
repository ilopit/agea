#!/usr/bin/env bash
# Format project C++ files using clang-format.
# Usage:
#   tools/format.sh          — format files changed since last commit
#   tools/format.sh --all    — format all project files
#   tools/format.sh --check  — dry-run, exit 1 if any file would change

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

CLANG_FORMAT="/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/x64/bin/clang-format.exe"
if [[ ! -x "$CLANG_FORMAT" ]]; then
    CLANG_FORMAT="$(command -v clang-format 2>/dev/null || true)"
fi
if [[ -z "$CLANG_FORMAT" ]]; then
    echo "ERROR: clang-format not found" >&2
    exit 1
fi

MODE="changed"
DRY_RUN=""

for arg in "$@"; do
    case "$arg" in
        --all)   MODE="all" ;;
        --check) DRY_RUN="--dry-run -Werror" ;;
        *)       echo "Unknown argument: $arg"; exit 1 ;;
    esac
done

# Directories to format (skip thirdparty, build, generated)
DIRS=(engine libs packages)

collect_all_files() {
    for dir in "${DIRS[@]}"; do
        find "$ROOT/$dir" -type f \( -name "*.h" -o -name "*.cpp" \) \
            ! -path "*/thirdparty/*" \
            ! -path "*/.ar.cpp" \
            ! -path "*/kryga_generated/*"
    done
}

collect_changed_files() {
    cd "$ROOT"
    git diff --name-only --diff-filter=d HEAD -- '*.h' '*.cpp' | while read -r f; do
        case "$f" in
            engine/*|libs/*|packages/*) echo "$ROOT/$f" ;;
        esac
    done
}

if [[ "$MODE" == "all" ]]; then
    FILES="$(collect_all_files)"
else
    FILES="$(collect_changed_files)"
fi

if [[ -z "$FILES" ]]; then
    echo "No files to format."
    exit 0
fi

COUNT=$(echo "$FILES" | wc -l)
echo "Formatting $COUNT file(s)..."

echo "$FILES" | xargs "$CLANG_FORMAT" -i --style=file $DRY_RUN

if [[ -n "$DRY_RUN" ]]; then
    echo "Check passed — all files formatted correctly."
else
    echo "Done."
fi
