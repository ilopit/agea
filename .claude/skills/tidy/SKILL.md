---
name: tidy
description: Run clang-tidy on changed C/C++ files using the curated .clang-tidy (bugs, naming, modernization, performance). Use when the user asks to run clang-tidy, lint for bugs, check naming conventions deterministically, or apply tidy auto-fixes. For whitespace/brace formatting use `format`; for judgment-based idiom review use `cpp-style`.
allowed-tools: Bash
---

Run clang-tidy via `tools/tidy.sh`, which wraps the curated `.clang-tidy` and
handles the MSVC env (vcvars) and the `host-tidy` Ninja compile DB.

## Commands

```
tools/tidy.sh                # changed-since-HEAD files, report only
tools/tidy.sh --fix          # apply auto-fixable diagnostics
tools/tidy.sh --all          # whole tree (slow — 800+ TUs)
tools/tidy.sh --configure    # (re)generate build_tidy/compile_commands.json, then run
```

## Instructions

1. First run: if `build_tidy/compile_commands.json` is missing the script errors —
   re-run with `--configure` once (Ninja configure, ~90s, downloads deps first time).
2. Default to report-only. Use `--fix` only when the user asks to apply fixes, and
   report which files changed afterward.
3. Summarize by check category (e.g. "12 modernize-type-traits, 3 missing-override"),
   not a raw dump. Point out the high-value ones (bugprone-*, clang-analyzer-*).
4. `--fix` can't apply overlapping fixes — if clang-tidy reports skipped overlaps,
   re-run once more.

## Notes / gotchas
- Needs a VS x64 toolchain; the script sources `vcvars64.bat` itself. Override with
  `CLANG_TIDY=` / `VCVARS=` if installed elsewhere.
- `host-tidy` disables PCH and module scanning (clang-tidy can't read MSVC `.pch`
  or build-time modmaps) — it does NOT affect normal `host`/Android builds.
- Generated `*.ar.*` and `thirdparty/` are excluded by config and by the script.
- The `name__suffix` double-underscore convention is allowed on purpose
  (`bugprone-reserved-identifier` is off). See the `cpp-style` skill's rules.md.
