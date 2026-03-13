#pragma once

#include "render_bridge/render_command.h"

#include <utils/id.h>

#include <unordered_map>

namespace kryga
{

namespace render
{
class vulkan_render;
class vulkan_render_loader;
class mesh_data;
class material_data;
class shader_effect_data;
class texture_data;
class vulkan_render_data;
class vulkan_directional_light_data;
class vulkan_universal_light_data;
}  // namespace render

class render_command_processor
{
public:
    render_command_processor();

    // Handle management (main thread, during lock-step idle)
    render_cmd::render_handle
    get_handle(const utils::id& id) const;

    render_cmd::render_handle
    ensure_handle(const utils::id& id);

    void
    erase_handle(const utils::id& id);

    // Resolve handle -> pointer (render thread, inside execute())
    render::mesh_data*
    resolve_mesh(render_cmd::render_handle h) const;
    render::texture_data*
    resolve_texture(render_cmd::render_handle h) const;
    render::shader_effect_data*
    resolve_shader_effect(render_cmd::render_handle h) const;
    render::material_data*
    resolve_material(render_cmd::render_handle h) const;
    render::vulkan_render_data*
    resolve_object(render_cmd::render_handle h) const;
    render::vulkan_directional_light_data*
    resolve_dir_light(render_cmd::render_handle h) const;
    render::vulkan_universal_light_data*
    resolve_univ_light(render_cmd::render_handle h) const;

    // Store resource pointer under handle (render thread, inside execute())
    void store_mesh(render_cmd::render_handle h, render::mesh_data* p);
    void store_texture(render_cmd::render_handle h, render::texture_data* p);
    void store_shader_effect(render_cmd::render_handle h, render::shader_effect_data* p);
    void store_material(render_cmd::render_handle h, render::material_data* p);
    void store_object(render_cmd::render_handle h, render::vulkan_render_data* p);
    void store_dir_light(render_cmd::render_handle h, render::vulkan_directional_light_data* p);
    void store_univ_light(render_cmd::render_handle h, render::vulkan_universal_light_data* p);

    // Erase resource from map (render thread, inside execute())
    void erase_mesh(render_cmd::render_handle h);
    void erase_texture(render_cmd::render_handle h);
    void erase_shader_effect(render_cmd::render_handle h);
    void erase_material(render_cmd::render_handle h);
    void erase_object(render_cmd::render_handle h);
    void erase_dir_light(render_cmd::render_handle h);
    void erase_univ_light(render_cmd::render_handle h);

    // Lookup by id (convenience, used by animation resolver etc.)
    render::mesh_data*
    get_mesh(const utils::id& id) const;
    render::texture_data*
    get_texture(const utils::id& id) const;
    render::shader_effect_data*
    get_shader_effect(const utils::id& id) const;
    render::material_data*
    get_material(const utils::id& id) const;
    render::vulkan_render_data*
    get_object(const utils::id& id) const;
    render::vulkan_directional_light_data*
    get_directional_light(const utils::id& id) const;
    render::vulkan_universal_light_data*
    get_universal_light(const utils::id& id) const;

private:
    std::unordered_map<utils::id, render_cmd::render_handle> m_handle_map;

    std::unordered_map<render_cmd::render_handle, render::mesh_data*> m_meshes;
    std::unordered_map<render_cmd::render_handle, render::texture_data*> m_textures;
    std::unordered_map<render_cmd::render_handle, render::shader_effect_data*> m_shader_effects;
    std::unordered_map<render_cmd::render_handle, render::material_data*> m_materials;
    std::unordered_map<render_cmd::render_handle, render::vulkan_render_data*> m_objects;
    std::unordered_map<render_cmd::render_handle, render::vulkan_directional_light_data*>
        m_dir_lights;
    std::unordered_map<render_cmd::render_handle, render::vulkan_universal_light_data*>
        m_univ_lights;

    render_cmd::render_handle m_next_handle = 0;
};

}  // namespace kryga
