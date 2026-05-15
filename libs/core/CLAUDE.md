# Core Library

Central runtime system: object construction, reflection, asset loading, container management.

## Architypes
8 object types defined in `architype.h`: smart_object, game_object, component, mesh, texture, shader_effect, material, sampler.

## Container hierarchy
`container` is the base for `package` and `level`. Manages object collections, caching via `cache_set`, VFS root mounting, and load contexts.
- Packages: static/dynamic, managed by `package_manager` (load/unload/save, registry of type builders)
- Levels: inherit from container, track game_objects + components with spawn capabilities, managed by `level_manager`
- VFS paths: enforced `"data://"` mount with `"packages/*.apkg"` and `"levels/*.alvl"` patterns

## Caching
Two-tier: global shared caches + per-container local `cache_set`. Maps assets by ID with typed specializations (objects, components, meshes, textures, materials, shader_effects, samplers). Use `unregister_in_global_cache` to remove.

## Reflection system
Located in `core/reflection/`. Type introspection via `type_resolver`, `reflection_type` registry, property handlers, function bindings. Operations pass context objects (`save_context`, `load_context`, `copy_context`, `compare_context`) enabling side-effect-free handlers.

## State mutators
`core_state.h` exposes `state_mutator__*` friend structs that inject systems (caches, level_manager, package_manager, reflection, Lua) into `gs::state`. This is the only way to write into global_state from core.

## Object construction
`object_constructor.h` — factory with explicit `smart_object_flags` tracking: instance, derived, runtime, mirror, default states.
