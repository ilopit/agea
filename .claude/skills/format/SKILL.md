---
name: format
description: Run clang-format on files changed since the last commit. Use when the user asks to format, lint, or clean up code style.
allowed-tools: Bash
---

Format all C/C++ files changed since the last commit using format_ar.py (for KRG_ar_* macros) and clang-format.

## Command

Run from the project root — format_ar.py first (on headers only), then clang-format on all changed files:

```
CLANG_FORMAT="/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/x64/bin/clang-format.exe"

# AR macro formatting (headers only, no-ops if no macros present)
git diff --name-only HEAD -- '*.h' | grep -v '^build/' | xargs -r python tools/format_ar.py
git ls-files --others --exclude-standard -- '*.h' | grep -v '^build/' | xargs -r python tools/format_ar.py

# clang-format
git diff --name-only HEAD -- '*.h' '*.cpp' | grep -v '^build/' | xargs -r "$CLANG_FORMAT" -i
git ls-files --others --exclude-standard -- '*.h' '*.cpp' | grep -v '^build/' | xargs -r "$CLANG_FORMAT" -i
```

When `--all` is passed, format ALL files (not just changed):

```
python tools/format_ar.py
tools/format.sh --all
```

## Instructions

1. Run both format_ar.py and clang-format in a single Bash call
2. Report how many files were formatted
