---
name: run
description: Run an executable or test binary from the build output directory. Use when the user asks to run the app, run tests, or execute a built binary.
argument-hint: "[-r] <executable> [args...]"
allowed-tools: Bash
---

Run from the project root:

```
tools/run.sh $ARGUMENTS
```

## Options

- `-r` — run Release build (default: Debug)
- No exe argument — lists available executables

## Examples

- `/run visual_regression_tests.exe`
- `/run vulkan_render_tests.exe --gtest_filter="render_device*"`
- `/run -r engine_app.exe`

## Instructions

1. Run the command with the provided arguments
2. If the test fails, report results clearly
3. Do NOT attempt to fix code unless explicitly asked
