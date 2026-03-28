---
name: format
description: Run clang-format on files changed since the last commit. Use when the user asks to format, lint, or clean up code style.
allowed-tools: Bash
---

Format all C/C++ files changed since the last commit using clang-format.

## Command

Run both commands from the project root to cover tracked changes and new files:

```
CLANG_FORMAT="/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/x64/bin/clang-format.exe"

git diff --name-only HEAD -- '*.h' '*.cpp' | grep -v '^thirdparty/upstream/' | grep -v '^build/' | xargs -r "$CLANG_FORMAT" -i

git ls-files --others --exclude-standard -- '*.h' '*.cpp' | grep -v '^thirdparty/upstream/' | grep -v '^build/' | xargs -r "$CLANG_FORMAT" -i
```

## Instructions

1. Run both commands in a single Bash call
2. Report how many files were formatted
