# Render Bridge

Translation layer between model objects (smart_object hierarchy) and Vulkan render data.

## Command system
render_bridge is **producer-only**: it creates commands from the model. Per-frame
arena allocation eliminates dynamic alloc overhead. Commands and arenas are
double-buffered by frame parity (slot = frame & 1): the main thread produces
frame F into slot (F&1) while the render thread consumes the other slot, so they
never touch the same queue at once.
1. `alloc_cmd<T>()` — allocate command from the active slot's arena
2. `enqueue_cmd()` — push onto the active slot's queue

Consuming a slot (`frame_pipeline::drain_frame(slot)` — execute that slot's whole
queue, then draw) is NOT here: it's the render-thread consumer side and lives on
`frame_pipeline` (engine). The queue itself is `render::input_queue`
(vulkan_render/input_queue.h), a **member of render_system** reached via
`getr_render().input_queue` — NOT part of `core::queues` (which now holds only
model-side dirty tracking). The slot *lifecycle* (`set_active_slot` / `reset_slot` /
`reset_arena`) lives on `render::input_queue`, driven by the frame owner
(`frame_pipeline` in the streaming loop, the headless tick otherwise). render_bridge
holds no slot state; it only builds commands into the active slot.

## Model-to-render dispatch (the render_bridge class)
The `render_bridge` class is **stateful** — it owns the command lifecycle:
- `render_cmd_build/destroy/transform()` — dispatch via reflection-based callbacks
- `get_dependency()` — the dependency graph (its only state)
- `alloc_cmd<T>()` / `enqueue_cmd()` — produce commands into the active arena/queue
- Reflection-driven `gpu_pack` / `render_cmd_builder` decouples model definitions from bridge logic

## Stateless translation (`render_translate.h`)
Pure model→render conversions live as free functions in `namespace render_translate`,
NOT on the class — they hold no state and touch no queue/arena:
- `collect_gpu_data()` — packs model data into GPU structs via reflection-based serialization
- `collect_spec_constants()` — gathers specialization constants from model properties
- `set_material_texture_bindings()` — wires texture/sampler indices
- `make_se_ci()` / `make_qid()` / `make_qid_from_model()` — render info from shader effects/materials/meshes
- `map_sampler_to_static_index()` — model sampler semantics → static sampler slot

Asset-format predicates (e.g. "is this a cooked `.atbc` texture") are NOT here —
they belong to the format owner: `asset_importer::texture_importer::is_kryga_texture()`.

## Dependency tracking
Dual dependency graphs: top-down (parent→child) and down-top (child→parent) for efficient hierarchical updates.

## Gotchas
- Texture binding fields initialized to `UINT32_MAX` (invalid) by default
- Arena is reset after drain — commands must not be retained across frames
