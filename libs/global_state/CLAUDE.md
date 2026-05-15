# Global State

Singleton central registry holding all engine subsystems as pointers. Access via `kryga::glob::glob_state()`.

## Lifecycle (4 stages, strictly sequential)
1. **create** — construct subsystems
2. **connect** — wire dependencies
3. **init** — initialize systems
4. **ready** — operational

Actions scheduled per-stage via `schedule_action()`, execute sequentially when `run_create/connect/init()` are called.

## State contents
- **Caches:** class/instance-level object, component, game_object, material, mesh, texture, sampler, shader caches
- **Managers:** level_manager, package_manager, id_generator
- **Reflection:** type registry, Lua API
- **Rendering:** vulkan_engine, render_device, render_loader, vulkan_render, render_bridge
- **Engine:** game_editor, input_manager, native_window, UI system, animation_system, engine_counters
- **VFS:** virtual_file_system
- **Generic:** `create_box<T>()` for extensible custom state

## Mutator pattern
Subsystems write into global_state via friend `state_mutator__*` structs. This is the only write path — maintains encapsulation while allowing distributed registration.

## Gotchas
- No thread safety — designed for single-threaded initialization
- `glob_state_reset()` reinitializes via move assignment
- Pointer members rely on external lifetime management — only boxes are explicitly cleaned
