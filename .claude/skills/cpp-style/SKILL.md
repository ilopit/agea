---
name: cpp-style
description: Review changed C++ for kryga's idiomatic conventions that clang-format CANNOT catch — naming, KRG_check asserts vs defensive null-checks, include order, class layout, namespaces, logging/reflection macros, C++23 idioms. Use when asked to review C++ style, check conventions/idioms, or audit code before committing. For pure whitespace/brace/alignment formatting use the `format` skill instead.
allowed-tools: Bash, Read, Grep, Edit
---

# C++ idiomatic style review

Semantic review, not formatting. clang-format already handles braces, indentation,
column limit, and alignment via the `format` skill — do NOT re-check those here.
This skill checks the conventions a formatter can't see: naming, error-handling
patterns, include order, class layout, and C++23 idioms.

If the code hasn't been mechanically formatted yet, suggest running the `format`
skill first, then run this.

## Procedure

1. **Collect targets.**
   - If the user named files (or another tool/agent handed you a specific set), review
     exactly those. This is the common case — use it and skip the git step.
   - Only if no targets were given, fall back to C++ changed since the last commit:
     ```
     git diff --name-only HEAD -- '*.h' '*.cpp' | grep -v '^build/'
     git ls-files --others --exclude-standard -- '*.h' '*.cpp' | grep -v '^build/'
     ```
   `thirdparty/` and `*.ar.h` / `*.ar.cpp` generated files are out of scope — skip them.

2. **Read each file** and check against the checklist below. Two references in this
   skill directory:
   - `examples.md` — good/bad code pairs; match code against the GOOD form, flag the
     BAD form with GOOD as the fix. Read this first; it's the fastest calibration.
   - `rules.md` — full rule set. Open when you're unsure a rule is real (not your
     assumption).

3. **Report, don't auto-edit.** Output one grouped list per file:
   `path:line — <rule violated> — <concrete fix>`. Lead with the count. If a file
   is clean, say so in one line.

4. **Fix only on confirmation.** Renames and removing defensive checks ripple
   across translation units — never apply them silently. After the report, ask
   which fixes to apply, then use Edit. Exception: the user explicitly said "fix"
   up front.

## Checklist (condensed — see `rules.md` for evidence)

**Precedence (read first)** — local convention in the same file/lib BEATS a generic
rule below. Before flagging an idiom-level issue (`[[nodiscard]]`, `std::size_t` vs
`size_t`, designated initializers), grep the surrounding lib: if the neighbors
consistently do otherwise, that IS the convention — match it, don't flag it. Only
flag when the change deviates from its own surroundings.

**Naming** — everything is `snake_case`: functions, locals, types/classes/structs,
enums (`enum class`) and enumerators, namespaces, files. Members are `m_`-prefixed,
statics `s_`-prefixed, snake_case. Macros are `UPPER_SNAKE` with `KRG_` prefix
(reflection markers `KRG_ar_*` are lowercase by design). Constants `snake_case`,
often `inline constexpr` / `static inline const`. `name__suffix` double-underscore
is an INTENTIONAL convention for internal qualifiers — never flag it.

**Semantic naming** (not just casing) — methods read verb-first (`rebuild_entries`,
not `entries_rebuild`); accessor/mutator names mirror the thing (`ordered_entries()`
↔ `rebuild_ordered_entries()`); avoid vague verbs (`do`/`handle`/`process`) without
a concrete object. This is the judgment layer clang-tidy can't reach — the point of
this skill.

**Error handling** — NO defensive null-checks or early-returns. Use `KRG_check(cond, msg)`
for invariants, `KRG_never(msg)` for unreachable, `KRG_not_implemented` for stubs.
Recoverable failures return `error_handling::result_code`. Flag any `if (ptr) return;`
guard that should be an assert.

**Includes** — `#pragma once` (never include guards). SortIncludes is OFF, so order
is manual and must stay: local `"subsystem/..."` quoted includes first, then
thirdparty/engine `<...>`, then std last. Don't reorder alphabetically.

**Namespaces** — under `kryga::<subsystem>::`. Closing braces carry a
`// namespace <name>` comment.

**Class layout** — access order public → friend → protected → private. `override`
on every virtual override. Ctor init lists one member per line.

**C++23 / idioms** — prefer `std::span` for array-view params, `std::optional` for
optional returns, `if constexpr` for compile-time dispatch, designated initializers
for aggregates, `[[nodiscard]]` on must-use returns (subject to Precedence above —
don't flag if the lib uniformly omits it). Use `<kryga_port/format.h>` not raw
`<format>`. Logging via `ALOG_TRACE/INFO/WARN/ERROR/FATAL`. Global state via
`glob::glob_state()`; prefer `getr_*()` (asserts) over nullable `get_*()`.

**Enums** — `enum class`, snake_case enumerators. CAUTION: if the enum is serialized
or persisted (stored on a model/component, saved to disk), it is effectively
append-only — inserting a value mid-list renumbers later values and breaks saved
data. New values go at the END unless you know it's not persisted.

**Reflected members (`KRG_ar_property`)** — the highest-frequency edit in `packages/`.
A reflected data member is wrapped in `// clang-format off` … `// clang-format on`
and its annotation block should MIRROR the adjacent reflected field (same keys, same
order: `category, access, serializable, invalidates, mcp_hint`). Header must be
registered with argen. For `mcp_hint` / annotation *wording* conventions, defer to
the `mcp-annotate` skill — don't re-derive them here.

## Out of scope (clang-format owns these — do not flag)
Brace placement (Allman), indent width (4), column limit (100), pointer alignment
(left), trailing whitespace, `else` on new line.
