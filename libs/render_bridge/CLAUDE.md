# Render Bridge

Translation layer between model objects (smart_object hierarchy) and Vulkan render data.

## Command system
Per-frame arena allocation eliminates dynamic alloc overhead. Commands and
arenas are double-buffered by frame parity (slot = frame & 1): the main thread
produces frame F into slot (F&1) while the render thread consumes the other
slot, so they never touch the same queue at once.
1. `alloc_cmd<T>()` — allocate command from the active slot's arena
2. `enqueue_cmd()` — push onto the active slot's queue
3. `drain_frame(slot)` — render thread executes that slot's whole queue, then draws
4. `reset_slot(slot)` — rewind the arena after the frame is drawn

## GPU data pipeline
- `collect_gpu_data()` — packs model data into GPU structs via reflection-based serialization
- `extract_gpu_data()` — extracts material properties via templates
- `collect_spec_constants()` — gathers specialization constants from model properties
- `set_material_texture_bindings()` — wires texture/sampler indices

## Model-to-render dispatch
- `render_cmd_build/destroy()` — dispatches via reflection-based callbacks
- `make_se_ci()` / `make_qid()` — creates render info from shader effects/materials/meshes
- Reflection-driven `gpu_pack` / `render_cmd_builder` decouples model definitions from bridge logic

## Dependency tracking
Dual dependency graphs: top-down (parent→child) and down-top (child→parent) for efficient hierarchical updates.

## Gotchas
- Texture binding fields initialized to `UINT32_MAX` (invalid) by default
- Arena is reset after drain — commands must not be retained across frames
