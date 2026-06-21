#pragma once

#include <cstdint>

namespace kryga
{

namespace render
{
class vulkan_render;
class vulkan_render_loader;
}  // namespace render

namespace render_cmd
{

// Tag discriminating each concrete render command. Replaces the old vtable: the
// processor's apply() switches on this to call the right execute() and destructor.
// Every command struct carries a `static constexpr k_kind`.
enum class render_cmd_kind : uint16_t
{
    update_transform,
    set_outline,
    create_mesh,
    destroy_mesh,
    create_texture,
    destroy_texture,
    create_shader_effect,
    destroy_shader_effect,
    create_material,
    update_material,
    destroy_material,
    create_object,
    update_object,
    destroy_object,
    create_chunk_mesh,
    create_light,
    update_light,
    select_directional_light,
    destroy_light,
    create_skinned_mesh,
    create_lightmap,
    destroy_lightmap,
    apply_bones,
};

struct render_exec_context
{
    render::vulkan_render& vr;
    render::vulkan_render_loader& loader;
};

// POD tagged header — NO vtable. Commands are arena-allocated PODs distinguished by
// `kind`; alloc_cmd<T> stamps it from T::k_kind. The drain never sees concrete types
// polymorphically — render_command_processor::apply switches on cmd_kind (the central
// tagged dispatch lives there, alongside the per-command process() handlers).
struct render_command_base
{
    // Named cmd_kind (not kind) to avoid colliding with command fields that are
    // themselves called `kind` (e.g. create_light_cmd's light_kind kind).
    render_cmd_kind cmd_kind;
};

}  // namespace render_cmd
}  // namespace kryga
