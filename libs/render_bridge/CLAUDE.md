# Render Bridge

Translation layer between model objects (smart_object hierarchy) and Vulkan render data.

## Command system
Per-frame arena allocation eliminates dynamic alloc overhead:
1. `alloc_cmd<T>()` — allocate command from arena
2. `enqueue_cmd()` — queue for render thread
3. `drain_queue()` — execute all queued commands
4. Arena reset after rendering

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
