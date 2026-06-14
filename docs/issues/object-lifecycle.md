# Object lifecycle — level vs class/package domains

**Type:** refactoring / tech debt
**Scope:** `libs/core` object cache & load context, level rollback, render teardown

## Issue

`object_load_context::add_obj` pushes **every** constructed object into the
container's ownable cache `m_objects` — both per-level **instances** and shared
**class objects (CDOs)** (`libs/core/private/src/object_load_context.cpp:66`).
This contradicts the intended model (the load-context header notes class objects
are shared/global while instances are "stored for lifetime only").

Consequence: when a type is first instantiated **during play** (e.g. grafting a
component via `model.component.add`, or spawning the player), its CDO is loaded
through the *level's* load context and lands in the level's `m_objects` — *after*
the rollback snapshot. `level::rollback()` sweeps `m_objects` by index, so it
scoops up a shared class object that is not per-play state.

### Effects

- **Crash (fixed by workaround):** rollback queued the CDO for `destroy_render`,
  and a runtime component's recursive destroy also reached shared CDO/package
  assets it merely references → `render_cmd_destroy` asserted
  `!default_obj` (`libs/render_translator/private/src/render_translator.cpp:253`),
  aborting the engine; the next `model.level.load` then hung against a dead
  process.
- **Wasteful/fragile (still present):** rollback `remove_obj`s + frees the
  mid-play CDO. Freeing a shared, readonly class object on every play exit is
  wasteful (reloaded on next use) and dangling-prone if any surviving instance
  still references it.

### Current workarounds (symptom-level, in place)

- `render_cmd_destroy` skips `default_obj` instead of asserting
  (`render_translator.cpp`).
- `is_same_source` requires a non-null shared owner so a level component does not
  recurse a render-destroy into package assets
  (`packages/base/private/src/render/overrides/render_types_handlers.cpp`).
- `level::rollback()` skips `default_obj` components when queuing `destroy_render`
  / detaching (`libs/core/private/src/level.cpp`).

These stop the crash but leave the root cause: class objects still enter the
level's instance set and are still freed by rollback.

## Proposed fix

Separate the two lifetime domains at the **object-cache layer**, not the render
queue:

- Route class/`default_obj` objects out of the container's ownable `m_objects`
  into the shared class/package domain (global cache only), so they persist
  across play sessions and level loads.
- `level::rollback()` then only ever sees per-level instances — the
  `default_obj` skips in rollback and `render_cmd_destroy` become unnecessary.
- Establish the invariant "the level's ownable set contains no class/package
  objects" and assert the inverse (no level instance leaks into package
  tracking) — catches both directions of the bug.

Note: this is **orthogonal to the render destroy queue**. Splitting
`destroy_render` into level/package queues would not have prevented the crash,
because the CDO is reached by *recursion* into referenced assets, not by the
top-level queue contents. The recursion guard (`is_same_source`) stays required;
the domain split belongs in `add_obj`/`cache_set`.

Affects: `object_load_context::add_obj`, `cache_set` / `caches_map`,
`container`/`level` object tracking, and lets the rollback + render-teardown
`default_obj` workarounds be removed.
