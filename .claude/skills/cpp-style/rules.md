# Kryga C++ conventions — full reference

Rules extracted from the codebase. Load when the condensed checklist in SKILL.md
isn't enough to judge a case; see `examples.md` for good/bad code pairs.

**Precedence:** local convention in the same file/lib overrides any generic rule
here. Grep the surroundings before flagging an idiom (`[[nodiscard]]`, `std::size_t`
vs `size_t`, etc.) — match what the neighbors do.

## 1. Naming

| Kind | Rule | Example |
|---|---|---|
| Functions | snake_case | `generate(const utils::id&)` |
| Locals | snake_case | `auto obj_id_raw = ...` |
| Members | `m_` prefix + snake_case | `m_snapshot` |
| Static variables | `s_` prefix + snake_case | `s_instance_count` |
| Classes/structs | snake_case | `class container` |
| Enums | `enum class`, snake_case | `enum class level_state` |
| Enumerators | snake_case | `unloaded = 0, loaded, render_loaded` |
| Constants | snake_case, `inline constexpr` / `static inline const` | `ks_class_default` |
| Namespaces | snake_case under `kryga::` | `kryga::core` |
| Macros | UPPER_SNAKE, `KRG_` prefix | `KRG_check` |
| Files | snake_case.h / .cpp | `id_generator.h` / `.cpp` |
| Internal qualifier | `name__suffix` (double underscore), snake_case | `property_handler__copy_guarded`; `state_mutator__*` |

**Semantic naming (beyond casing).** Methods verb-first; accessor/mutator names
mirror the entity (`ordered_entries()` ↔ `rebuild_ordered_entries()`); avoid vague
verbs (`do`/`handle`/`process`) without a concrete object.

**Enum caution.** Serialized/persisted `enum class` (stored on a model/component or
saved to disk) is effectively append-only — inserting a value mid-list renumbers
later values and corrupts saved data. Append new values at the end.

**The `__` convention is intentional.** Double underscore separates an entity from
an internal qualifier/variant (e.g. `property_handler__copy_guarded`,
`property_context__save`, `state_mutator__core`). It marks "internal / generated /
guarded" detail explicitly. Yes, `__` is technically reserved by the C++ standard;
the team accepts this for explicitness. `bugprone-reserved-identifier` is disabled
in `.clang-tidy` accordingly. Do NOT flag `__` names — flag only `_Capital` or a
leading `_` at global scope, which are not part of this convention.

## 2. Headers
- `#pragma once` always — no include guards.
- SortIncludes is OFF in `.clang-format`. Manual order: local quoted
  `"subsystem/..."` first, then thirdparty/engine `<...>`, then std last.
- Public headers in `public/include/<subsystem>/`; private in `private/src/` or
  `private/include/`.

## 3. Error handling
- `KRG_check(condition, msg)` for invariants — NOT defensive `if`-checks.
- `KRG_never(msg)` unreachable; `KRG_not_implemented` stubs.
- No defensive null-checks / early-returns (CLAUDE.md rule). Assert instead.
- Recoverable errors: `error_handling::result_code` enum.

## 4. Class layout
- Access order: public → friend → protected → private.
- Members grouped by type; pointers/refs before primitives.
- `override` on every virtual override.
- Ctor init lists: one member per line (clang-format enforced).

## 5. C++23 idioms actually used
- `std::span<const T>` for array-view params.
- `std::optional<T>` optional returns.
- `if constexpr` compile-time dispatch.
- `[[nodiscard]]` on must-use returns.
- Designated initializers for aggregates.
- `std::format` via the `kryga_port` format shim (Android portability), not raw
  `<format>`.

## 6. Namespaces
- Root `kryga::`, nested `kryga::<subsystem>::`.
- Closing brace comments: `} // namespace core } // namespace kryga`.

## 7. Project-specific macros / patterns
- Reflection: `KRG_ar_class/property/function(...)` parsed by argen. New reflected
  headers must be registered (see memory: argen config). Don't read generated
  `*.ar.cpp` without permission.
- Reflected data members: wrap each in `// clang-format off` … `// clang-format on`;
  mirror the adjacent reflected field's annotation keys and order
  (`category, access, serializable, invalidates, mcp_hint`). For annotation *wording*
  defer to the `mcp-annotate` skill — don't re-derive it here.
- Class gen: `KRG_gen_class_meta`, `KRG_gen_construct_params`, `KRG_gen_meta_api`.
- Singleton: `glob::glob_state()`; `getr_*()` asserts non-null, `get_*()` nullable.
  Prefer `getr_*()`.
- Logging: `ALOG_TRACE/INFO/WARN/ERROR/FATAL(...)` (spdlog wrappers).
- Enum decl: `KRG_declare_enum_simple(name, underlying, MACRO_LIST)`.
