#!/usr/bin/env bash
# Format project C++ files using clang-format.
# Usage:
#   tools/format.sh          — format files changed since last commit
#   tools/format.sh --all    — format all project files
#   tools/format.sh --check  — dry-run, exit 1 if any file would change

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# clang-format discovery — no hardcoded VS version/edition/drive. Order: $CLANG_FORMAT
# env, then the VS-bundled copy (located via vswhere; preferred for deterministic
# formatting), then PATH. On non-Windows, vswhere is absent and it falls through to PATH.
find_vswhere() {
    local pf86
    pf86="$(cygpath -u "$(printenv 'ProgramFiles(x86)' 2>/dev/null)" 2>/dev/null || true)"
    [[ -n "$pf86" ]] || pf86="/c/Program Files (x86)"
    local vw="$pf86/Microsoft Visual Studio/Installer/vswhere.exe"
    [[ -x "$vw" ]] && { printf '%s\n' "$vw"; return 0; }
    command -v vswhere.exe 2>/dev/null
}

if [[ -z "${CLANG_FORMAT:-}" ]]; then
    vsw="$(find_vswhere || true)"
    if [[ -n "$vsw" ]]; then
        vsi="$("$vsw" -latest -products '*' \
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 \
            -property installationPath 2>/dev/null | tr -d '\r')"
        cand="$(cygpath -u "$vsi")/VC/Tools/Llvm/x64/bin/clang-format.exe"
        [[ -x "$cand" ]] && CLANG_FORMAT="$cand"
    fi
    [[ -n "${CLANG_FORMAT:-}" ]] || CLANG_FORMAT="$(command -v clang-format 2>/dev/null || true)"
fi
if [[ -z "${CLANG_FORMAT:-}" ]]; then
    echo "ERROR: clang-format not found. Set CLANG_FORMAT= or install the VS 'C++ Clang tools'." >&2
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
    { git diff --name-only --diff-filter=d HEAD -- '*.h' '*.cpp'
      git ls-files --others --exclude-standard -- '*.h' '*.cpp'; } | sort -u | while read -r f; do
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
