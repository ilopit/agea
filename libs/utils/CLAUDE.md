# Utils

Foundational types used across the entire engine.

## ID system (`id.h`, `id_allocator.h`)
- Fixed 63-byte string-based ID with hash specialization
- `AID("name")` macro for creation
- Empty ID (`m_id[0] == '\0'`) means invalid — no separate boolean flag
- `id_allocator`: numeric ID reuse pool with free list

## Allocators
- `memory_arena` — linear bump allocator, 4 MB default, bulk `reset()`. Reset does NOT call destructors — objects must be trivial or manually destroyed.

## Assertion macros (`check.h`)
- `KRG_check(expr)` — assert, used instead of defensive null-checks
- `KRG_never` — unreachable code
- `KRG_not_implemented` — placeholder for unfinished code

## Dynamic objects (`dynamic_object.h`)
- `dynobj`: type-erased raw binary buffer + layout
- `dynobj_view`: typed accessor requiring explicit `TYPE_DESCRIPTOR` template
- Supports arrays, nested objects, variadic multi-field operations

## Other utilities
- `path.h` — `std::filesystem::path` wrapper with automatic `lexically_normal()` on every operation
- `buffer.h` — raw byte buffer with file I/O and typed views
- `kryga_log.h` — spdlog wrapper: `ALOG_TRACE/INFO/WARN/ERROR/FATAL`. Must initialize before use.
- `singleton_instance.h` — singleton patterns
- `string_utility.h` — split, prefix/suffix checks
