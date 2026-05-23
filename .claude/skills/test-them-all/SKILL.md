---
name: test-them-all
description: Run all tests in sequence — unit tests first, then visual regression, then e2e in parallel. Use when the user asks to run all tests, full test suite, or verify everything works.
argument-hint: "[--skip-unit] [--skip-visual] [--skip-e2e] [--filter <pattern>]"
allowed-tools: Bash
---

Run the full test suite in order: unit → visual regression → e2e (parallel).

Each phase must pass before the next starts. Report failures immediately — do NOT continue to later phases if an earlier one fails.

## Phase 1: Unit tests (GTest binaries, sequential)

Build and run all GTest binaries:

```bash
tools/build.sh -j 16 -a && \
tools/run.sh model_tests.exe && \
tools/run.sh packages.root.model.tests.exe && \
tools/run.sh packages.base.model.tests.exe && \
tools/run.sh packages.tbs.model.tests.exe && \
tools/run.sh packages.test.model.tests.exe && \
tools/run.sh utils_tests.exe && \
tools/run.sh vfs_tests.exe && \
tools/run.sh render_utils_tests.exe && \
tools/run.sh shader_system_tests.exe && \
tools/run.sh asset_converter_tests.exe && \
tools/run.sh vulkan_render_tests.exe
```

## Phase 2: Visual regression (requires GPU)

```bash
tools/run.sh visual_regression_tests.exe
```

If tests fail, report which tests and pixel diff % — do NOT update references.

## Phase 3: E2E regression (pytest, parallel via xdist)

```bash
python -m pytest tests/e2e/ -v -x -n auto
```

- `-n auto` runs tests in parallel across workers (each gets its own engine instance)
- `-x` stops on first failure
- If `pytest-xdist` is not installed, fall back to serial: `python -m pytest tests/e2e/ -v -x`

## Skipping phases

If arguments contain:
- `--skip-unit` — skip phase 1
- `--skip-visual` — skip phase 2
- `--skip-e2e` — skip phase 3
- `--filter <pattern>` — pass as `--gtest_filter=<pattern>` to unit tests

## Reporting

After all phases complete (or on failure), report:

```
Phase 1 (unit):       X/Y passed
Phase 2 (visual):     X/Y passed
Phase 3 (e2e):        X/Y passed
```
