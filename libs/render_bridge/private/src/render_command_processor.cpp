#include "render_bridge/render_command_processor.h"

namespace kryga
{

render_command_processor::render_command_processor() = default;

// ============================================================================
// Handle management
// ============================================================================

render_cmd::render_handle
render_command_processor::get_handle(const utils::id& id) const
{
    auto it = m_handle_map.find(id);
    return it != m_handle_map.end() ? it->second : render_cmd::k_invalid_handle;
}

render_cmd::render_handle
render_command_processor::ensure_handle(const utils::id& id)
{
    auto existing = get_handle(id);
    if (existing != render_cmd::k_invalid_handle)
    {
        return existing;
    }

    auto h = m_next_handle++;
    m_handle_map[id] = h;
    return h;
}

void
render_command_processor::erase_handle(const utils::id& id)
{
    m_handle_map.erase(id);
}

// ============================================================================
// Resolve handle -> pointer
// ============================================================================

render::mesh_data*
render_command_processor::resolve_mesh(render_cmd::render_handle h) const
{
    auto it = m_meshes.find(h);
    return it != m_meshes.end() ? it->second : nullptr;
}

render::texture_data*
render_command_processor::resolve_texture(render_cmd::render_handle h) const
{
    auto it = m_textures.find(h);
    return it != m_textures.end() ? it->second : nullptr;
}

render::shader_effect_data*
render_command_processor::resolve_shader_effect(render_cmd::render_handle h) const
{
    auto it = m_shader_effects.find(h);
    return it != m_shader_effects.end() ? it->second : nullptr;
}

render::material_data*
render_command_processor::resolve_material(render_cmd::render_handle h) const
{
    auto it = m_materials.find(h);
    return it != m_materials.end() ? it->second : nullptr;
}

render::vulkan_render_data*
render_command_processor::resolve_object(render_cmd::render_handle h) const
{
    auto it = m_objects.find(h);
    return it != m_objects.end() ? it->second : nullptr;
}

render::vulkan_directional_light_data*
render_command_processor::resolve_dir_light(render_cmd::render_handle h) const
{
    auto it = m_dir_lights.find(h);
    return it != m_dir_lights.end() ? it->second : nullptr;
}

render::vulkan_universal_light_data*
render_command_processor::resolve_univ_light(render_cmd::render_handle h) const
{
    auto it = m_univ_lights.find(h);
    return it != m_univ_lights.end() ? it->second : nullptr;
}

// ============================================================================
// Store resource pointer
// ============================================================================

void render_command_processor::store_mesh(render_cmd::render_handle h, render::mesh_data* p) { m_meshes[h] = p; }
void render_command_processor::store_texture(render_cmd::render_handle h, render::texture_data* p) { m_textures[h] = p; }
void render_command_processor::store_shader_effect(render_cmd::render_handle h, render::shader_effect_data* p) { m_shader_effects[h] = p; }
void render_command_processor::store_material(render_cmd::render_handle h, render::material_data* p) { m_materials[h] = p; }
void render_command_processor::store_object(render_cmd::render_handle h, render::vulkan_render_data* p) { m_objects[h] = p; }
void render_command_processor::store_dir_light(render_cmd::render_handle h, render::vulkan_directional_light_data* p) { m_dir_lights[h] = p; }
void render_command_processor::store_univ_light(render_cmd::render_handle h, render::vulkan_universal_light_data* p) { m_univ_lights[h] = p; }

// ============================================================================
// Erase resource
// ============================================================================

void render_command_processor::erase_mesh(render_cmd::render_handle h) { m_meshes.erase(h); }
void render_command_processor::erase_texture(render_cmd::render_handle h) { m_textures.erase(h); }
void render_command_processor::erase_shader_effect(render_cmd::render_handle h) { m_shader_effects.erase(h); }
void render_command_processor::erase_material(render_cmd::render_handle h) { m_materials.erase(h); }
void render_command_processor::erase_object(render_cmd::render_handle h) { m_objects.erase(h); }
void render_command_processor::erase_dir_light(render_cmd::render_handle h) { m_dir_lights.erase(h); }
void render_command_processor::erase_univ_light(render_cmd::render_handle h) { m_univ_lights.erase(h); }

// ============================================================================
// Lookup by id (convenience)
// ============================================================================

render::mesh_data*
render_command_processor::get_mesh(const utils::id& id) const
{
    auto h = get_handle(id);
    if (h == render_cmd::k_invalid_handle)
        return nullptr;
    return resolve_mesh(h);
}

render::texture_data*
render_command_processor::get_texture(const utils::id& id) const
{
    auto h = get_handle(id);
    if (h == render_cmd::k_invalid_handle)
        return nullptr;
    return resolve_texture(h);
}

render::shader_effect_data*
render_command_processor::get_shader_effect(const utils::id& id) const
{
    auto h = get_handle(id);
    if (h == render_cmd::k_invalid_handle)
        return nullptr;
    return resolve_shader_effect(h);
}

render::material_data*
render_command_processor::get_material(const utils::id& id) const
{
    auto h = get_handle(id);
    if (h == render_cmd::k_invalid_handle)
        return nullptr;
    return resolve_material(h);
}

render::vulkan_render_data*
render_command_processor::get_object(const utils::id& id) const
{
    auto h = get_handle(id);
    if (h == render_cmd::k_invalid_handle)
        return nullptr;
    return resolve_object(h);
}

render::vulkan_directional_light_data*
render_command_processor::get_directional_light(const utils::id& id) const
{
    auto h = get_handle(id);
    if (h == render_cmd::k_invalid_handle)
        return nullptr;
    return resolve_dir_light(h);
}

render::vulkan_universal_light_data*
render_command_processor::get_universal_light(const utils::id& id) const
{
    auto h = get_handle(id);
    if (h == render_cmd::k_invalid_handle)
        return nullptr;
    return resolve_univ_light(h);
}

}  // namespace kryga
