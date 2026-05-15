# Reflection System — Known Issues

## 1. No flag to mark objects as editable but not saveable [medium, architecture]

The reflection system has `serializable` on properties, but no equivalent flag on objects. There is no way to express "this object can be edited at runtime but should not be persisted via `object_save`."

**Where it matters**: Temporary/transient objects created for editor workflows — e.g. material editing instances created via `object_constructor::instantiate_obj()`. These instances are real objects in the OLC with full reflection, but saving them is meaningless or harmful (they have synthetic IDs like `__edit_mt_red`, no valid VFS path, and exist only for the duration of an editing session).

Currently the caller must remember not to call `object_save` on these objects. Nothing in the object itself signals its transience.

**Effect**: Any code that walks objects generically (auto-save, dirty tracking, undo stack, bulk export) must manually exclude transient objects by convention. One missed check = corrupt .aobj or crash from missing VFS path.

**Proposed fix — object-level flag**:

Add a flag to `smart_object` (or `object_flags`) like `transient` / `no_save`. Set it during `instantiate_obj` when the caller signals temporary intent, or let callers set it after creation.

```cpp
// object_flags or smart_object
bool transient = false;  // skip in object_save, auto-save, dirty tracking
```

`object_save` / `object_save_internal` would check and early-return (or assert) if the flag is set. Editor-side systems (undo, auto-save) would skip flagged objects.

**Alternative — separate cache**: Keep transient objects in a dedicated cache outside the OLC entirely, so they never mix with persistable objects. Heavier refactor, but eliminates the problem structurally instead of with a flag.

## 2. `serializable=false` properties appear editable in inspector [low, resolved]

Properties with `serializable=false` + `gpu_data=MaterialData` (e.g. `simple_texture_material::m_diffuse`) had `json_load` handlers and appeared editable in the inspector, but changes could never persist — `object_save` / `diff_object_properties` only iterate `m_serialization_properties`.

**Status**: Fixed — `encode_owner` in `property_rpc.cpp` now checks `p->serializable` and marks non-serializable fields as `readonly`. The `save_material_edit` path also skips non-serializable properties during instance→class copy.

**Files**: `engine/private/src/property_rpc.cpp:81`, `engine/private/src/ui/material_previewer.cpp` (save_material_edit).
