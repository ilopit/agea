---
name: format
description: Run clang-format on files changed since the last commit. Use when the user asks to format, lint, or clean up code style.
allowed-tools: Bash
---

Format all C/C++ files changed since the last commit using format_ar.py (for KRG_ar_* macros) and clang-format.

## Command

Run from the project root — format_ar.py first (AR macros, headers only), then
`tools/format.sh` for clang-format. The clang-format path is auto-discovered via
vswhere inside format.sh (no hardcoded VS path); it formats changed + untracked files.

```
# AR macro formatting (headers only, no-ops if no macros present)
git diff --name-only HEAD -- '*.h' | grep -v '^build/' | xargs -r python tools/format_ar.py
git ls-files --others --exclude-standard -- '*.h' | grep -v '^build/' | xargs -r python tools/format_ar.py

# clang-format (changed + untracked)
tools/format.sh
```

When `--all` is passed, format ALL files (not just changed):

```
python tools/format_ar.py
tools/format.sh --all
```

## Instructions

1. Run both format_ar.py and clang-format in a single Bash call
2. Report how many files were formatted
