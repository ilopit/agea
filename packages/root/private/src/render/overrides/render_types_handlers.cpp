#include "packages/root/package.root.h"

#include <global_state/global_state.h>
#include <render_translator/render_translator.h>
#include <render_translator/render_convert.h>
#include <render_translator/render_command.h>
#include <vulkan_render/render_system.h>  // getr_render().ctx.rb->meshes_alloc().reserve()

#include "packages/root/model/assets/mesh.h"
#include "packages/root/model/assets/material.h"
#include "packages/root/model/assets/texture_slot.h"
#include "packages/root/model/assets/texture.h"
#include "packages/root/model/assets/sampler.h"
#include "packages/root/model/assets/shader_effect.h"

#include <gpu_types/gpu_generic_constants.h>
#include <glue/type_ids.ar.h>

#include "packages/root/model/game_object.h"

#include <core/reflection/reflection_type.h>
#include <core/reflection/property_utils.h>
#include <core/caches/cache_set.h>
#include <core/object_constructor.h>
#include <core/package.h>
#include <core/reflection/reflection_type_utils.h>

#include <limits>

#include <serialization/serialization.h>

#include <assets_importer/mesh_importer.h>
#include <assets_importer/texture_importer.h>

#include <vulkan_render/utils/vulkan_initializers.h>
#include <vulkan_render/types/vulkan_mesh_data.h>
#include <vulkan_render/types/vulkan_texture_data.h>
#include <vulkan_render/types/vulkan_material_data.h>
#include <vulkan_render/types/vulkan_gpu_types.h>
#include <vulkan_render/types/vulkan_shader_effect_data.h>
#include <vulkan_render/types/vulkan_shader_data.h>
#include <vulkan_render/vulkan_render_loader.h>
#include <vulkan_render/vulkan_render_device.h>
#include <vulkan_render/kryga_render.h>

#include <vulkan_render/types/vulkan_render_pass.h>

#include <utils/kryga_log.h>
#include <utils/string_utility.h>
#include <utils/dynamic_object_builder.h>

namespace kryga
{

namespace root
{

// ============================================================================
// Render commands
// ============================================================================

struct create_mesh_cmd : render_cmd::render_command_base
{
    utils::id id;
    render::types::mesh_handle handle;  // pre-reserved by the builder (handle model)
    std::shared_ptr<utils::buffer> vertices;
    std::shared_ptr<utils::buffer> indices;
    bool skinned = false;

    void
    execute(render_cmd::render_exec_context& ctx) override
    {
        // Populate the slot the builder pre-reserved. Handle-only — no id index.
        if (skinned)
        {
            auto vbv = vertices->make_view<gpu::skinned_vertex_data>();
            auto ibv = indices->make_view<gpu::uint>();
            ctx.loader.populate_skinned_mesh(handle, id, vbv, ibv);
        }
        else
        {
            auto vbv = vertices->make_view<gpu::vertex_data>();
            auto ibv = indices->make_view<gpu::uint>();
            ctx.loader.populate_mesh(handle, id, vbv, ibv);
        }
    }
};

struct destroy_mesh_cmd : render_cmd::render_command_base
{
    render::types::mesh_handle handle;

    void
    execute(render_cmd::render_exec_context& ctx) override
    {
        // [render thread] Release the data; the model already free()'d the slot.
        ctx.loader.reset_mesh_storage(handle);
    }
};

struct create_texture_cmd : render_cmd::render_command_base
{
    utils::id id;
    render::types::texture_handle handle;  // pre-reserved by the builder
    std::shared_ptr<utils::buffer> pixels;
    uint32_t width = 0;
    uint32_t height = 0;
    bool is_kryga_format = false;

    void
    execute(render_cmd::render_exec_context& ctx) override
    {
        ctx.loader.populate_texture(handle, id, *pixels, width, height);
    }
};

struct destroy_texture_cmd : render_cmd::render_command_base
{
    render::types::texture_handle handle;

    void
    execute(render_cmd::render_exec_context& ctx) override
    {
        ctx.loader.reset_texture_storage(handle);
    }
};

struct create_shader_effect_cmd : render_cmd::render_command_base
{
    utils::id id;
    std::shared_ptr<utils::buffer> vert;
    std::shared_ptr<utils::buffer> frag;
    bool is_vert_binary = false;
    bool is_frag_binary = false;
    bool wire_topology = false;
    bool enable_alpha = false;
    std::unordered_map<std::string, uint32_t> spec_constants;

    void
    execute(render_cmd::render_exec_context& ctx) override
    {
        auto* rp = ctx.vr.get_render_pass(AID("main"));
        auto se_data = rp->get_shader_effect(id);

        if (!se_data)
        {
            render::shader_effect_create_info se_ci;
            se_ci.vert_buffer = vert.get();
            se_ci.frag_buffer = frag.get();
            se_ci.is_vert_binary = is_vert_binary;
            se_ci.is_frag_binary = is_frag_binary;
            se_ci.is_wire = wire_topology;
            se_ci.alpha = enable_alpha ? render::alpha_mode::world : render::alpha_mode::none;
            se_ci.rp = rp;
            se_ci.enable_dynamic_state = false;
            se_ci.ds_mode = render::depth_stencil_mode::none;
            se_ci.cull_mode = se_ci.is_wire ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;
            se_ci.spec_constants = spec_constants;

            rp->create_shader_effect(id, se_ci, se_data);
        }
    }
};

struct destroy_shader_effect_cmd : render_cmd::render_command_base
{
    utils::id id;

    void
    execute(render_cmd::render_exec_context& ctx) override
    {
        auto* rp = ctx.vr.get_render_pass(AID("main"));
        auto* se_data = rp->get_shader_effect(id);
        if (se_data)
        {
            if (auto* owner_rp = se_data->get_owner_render_pass())
            {
                owner_rp->destroy_shader_effect(id);
            }
        }
    }
};

struct texture_slot_info
{
    uint32_t slot = 0;
    render::types::texture_handle texture_handle;
    uint8_t static_sampler_index = 0;
};

struct create_material_cmd : render_cmd::render_command_base
{
    utils::id id;
    render::types::material_handle handle;  // pre-reserved by the builder (handle model)
    utils::id type_id;
    utils::id shader_effect_id;
    std::vector<texture_slot_info> texture_slots;
    utils::dynobj gpu_data;

    void
    execute(render_cmd::render_exec_context& ctx) override
    {
        auto* rp = ctx.vr.get_render_pass(AID("main"));
        auto* se_data = rp->get_shader_effect(shader_effect_id);

        if (!se_data || se_data->m_failed_load)
        {
            se_data = rp->get_shader_effect(AID("se_error"));
        }

        std::vector<render::texture_sampler_data> samples;
        for (auto& slot : texture_slots)
        {
            if (slot.texture_handle)
            {
                auto* td = ctx.loader.get_texture_data(slot.texture_handle);
                if (td)
                {
                    render::texture_sampler_data tsd;
                    tsd.texture = td;
                    tsd.slot = slot.slot;
                    samples.push_back(tsd);
                }
            }
        }

        uint32_t gpu_texture_indices[KGPU_MAX_TEXTURE_SLOTS];
        uint32_t gpu_sampler_indices[KGPU_MAX_TEXTURE_SLOTS];
        for (int i = 0; i < KGPU_MAX_TEXTURE_SLOTS; ++i)
        {
            gpu_texture_indices[i] = UINT32_MAX;
            gpu_sampler_indices[i] = 0;
        }

        for (auto& slot : texture_slots)
        {
            if (slot.texture_handle && slot.slot < KGPU_MAX_TEXTURE_SLOTS)
            {
                auto* td = ctx.loader.get_texture_data(slot.texture_handle);
                if (td)
                {
                    gpu_texture_indices[slot.slot] = td->get_bindless_index();
                }
            }
            if (slot.slot < KGPU_MAX_TEXTURE_SLOTS)
            {
                gpu_sampler_indices[slot.slot] = slot.static_sampler_index;
            }
        }

        render_convert::set_material_texture_bindings(
            gpu_data, gpu_texture_indices, gpu_sampler_indices, KGPU_MAX_TEXTURE_SLOTS);

        // Handle path: populate the slot the builder pre-reserved, then read
        // back for the bindless-sampler setup below.
        ctx.loader.populate_material(handle, id, type_id, samples, *se_data, gpu_data);
        auto* mat_data = ctx.loader.get_material_data(handle);

        if (mat_data)
        {
            for (auto& slot : texture_slots)
            {
                if (slot.slot < KGPU_MAX_TEXTURE_SLOTS)
                {
                    mat_data->set_bindless_sampler_index(slot.slot, slot.static_sampler_index);
                }
            }

            if (!gpu_data.empty())
            {
                ctx.vr.stage_add_material(mat_data);
            }
        }
    }
};

struct update_material_cmd : render_cmd::render_command_base
{
    render::types::material_handle handle;
    utils::id shader_effect_id;
    std::vector<texture_slot_info> texture_slots;
    utils::dynobj gpu_data;

    void
    execute(render_cmd::render_exec_context& ctx) override
    {
        if (!ctx.loader.material_valid(handle))
        {
            return;
        }
        auto* mat_data = ctx.loader.get_material_data(handle);
        if (!mat_data)
        {
            return;
        }

        auto* rp = ctx.vr.get_render_pass(AID("main"));
        auto* se_data = rp->get_shader_effect(shader_effect_id);

        if (!se_data || se_data->m_failed_load)
        {
            se_data = rp->get_shader_effect(AID("se_error"));
        }

        std::vector<render::texture_sampler_data> samples;
        for (auto& slot : texture_slots)
        {
            if (slot.texture_handle)
            {
                auto* td = ctx.loader.get_texture_data(slot.texture_handle);
                if (td)
                {
                    render::texture_sampler_data tsd;
                    tsd.texture = td;
                    tsd.slot = slot.slot;
                    samples.push_back(tsd);
                }
            }
        }

        uint32_t gpu_texture_indices[KGPU_MAX_TEXTURE_SLOTS];
        uint32_t gpu_sampler_indices[KGPU_MAX_TEXTURE_SLOTS];
        for (int i = 0; i < KGPU_MAX_TEXTURE_SLOTS; ++i)
        {
            gpu_texture_indices[i] = UINT32_MAX;
            gpu_sampler_indices[i] = 0;
        }

        for (auto& slot : texture_slots)
        {
            if (slot.texture_handle && slot.slot < KGPU_MAX_TEXTURE_SLOTS)
            {
                auto* td = ctx.loader.get_texture_data(slot.texture_handle);
                if (td)
                {
                    gpu_texture_indices[slot.slot] = td->get_bindless_index();
                }
            }
            if (slot.slot < KGPU_MAX_TEXTURE_SLOTS)
            {
                gpu_sampler_indices[slot.slot] = slot.static_sampler_index;
            }
        }

        render_convert::set_material_texture_bindings(
            gpu_data, gpu_texture_indices, gpu_sampler_indices, KGPU_MAX_TEXTURE_SLOTS);

        ctx.loader.update_material(*mat_data, samples, *se_data, gpu_data);

        for (auto& slot : texture_slots)
        {
            if (slot.slot < KGPU_MAX_TEXTURE_SLOTS)
            {
                mat_data->set_bindless_sampler_index(slot.slot, slot.static_sampler_index);
            }
        }

        if (!gpu_data.empty())
        {
            ctx.vr.stage_update_material(mat_data);
        }
    }
};

struct destroy_material_cmd : render_cmd::render_command_base
{
    render::types::material_handle handle;

    void
    execute(render_cmd::render_exec_context& ctx) override
    {
        if (!ctx.loader.material_valid(handle))
        {
            return;
        }
        auto* mat_data = ctx.loader.get_material_data(handle);
        if (mat_data)
        {
            ctx.vr.stage_remove_material(mat_data);
            // [render thread] Release the data; the model already free()'d the slot.
            ctx.loader.reset_material_storage(handle);
        }
    }
};

// ============================================================================
// Command builders
// ============================================================================

result_code
mesh__cmd_builder(reflection::type_context__render_cmd_build& ctx)
{
    auto& msh_model = ctx.obj->asr<root::mesh>();

    auto vertices = msh_model.get_vertices_buffer().make_view<gpu::vertex_data>();
    auto indices = msh_model.get_indices_buffer().make_view<gpu::uint>();

    if (!msh_model.get_vertices_buffer().size())
    {
        if (!asset_importer::mesh_importer::extract_mesh_data_from_3do(
                msh_model.get_external_buffer().get_file(), vertices, indices))
        {
            ALOG_LAZY_ERROR;
            return result_code::failed;
        }
    }

    auto vbv = msh_model.get_vertices_buffer().make_view<gpu::vertex_data>();
    glm::vec3 vmin{std::numeric_limits<float>::max()};
    glm::vec3 vmax{std::numeric_limits<float>::lowest()};
    for (size_t i = 0; i < vbv.size(); ++i)
    {
        const auto& pos = vbv.at(i).position;
        vmin = glm::min(vmin, pos);
        vmax = glm::max(vmax, pos);
    }
    glm::vec3 centroid = (vmin + vmax) * 0.5f;
    float max_dist_from_centroid_sq = 0.0f;
    for (size_t i = 0; i < vbv.size(); ++i)
    {
        glm::vec3 d = vbv.at(i).position - centroid;
        max_dist_from_centroid_sq = std::max(max_dist_from_centroid_sq, glm::dot(d, d));
    }
    msh_model.set_bounding_radius(std::sqrt(max_dist_from_centroid_sq));
    msh_model.set_local_centroid({centroid.x, centroid.y, centroid.z});

    // Reserve the render slot on the model thread and park the handle on the
    // asset; the create command populates it on the render thread.
    auto& loader = glob::glob_state().getr_render().loader;
    msh_model.set_render_handle(ctx.rb->meshes_alloc().reserve());

    auto* cmd = ctx.rb->alloc_cmd<create_mesh_cmd>();
    cmd->id = msh_model.get_id();
    cmd->handle = msh_model.render_handle();
    cmd->vertices = std::make_shared<utils::buffer>(msh_model.get_vertices_buffer());
    cmd->indices = std::make_shared<utils::buffer>(msh_model.get_indices_buffer());
    cmd->skinned = false;

    msh_model.set_render_built(true);
    ctx.rb->enqueue_cmd(cmd);

    return result_code::ok;
}

result_code
mesh__cmd_destroyer(reflection::type_context__render_cmd_build& ctx)
{
    auto& msh_model = ctx.obj->asr<root::mesh>();

    if (msh_model.get_render_built())
    {
        auto handle = msh_model.render_handle();
        // [model thread] Free the allocator slot now (deferred recycle); the
        // command releases the render-side data.
        ctx.rb->meshes_alloc().free(handle);

        auto* cmd = ctx.rb->alloc_cmd<destroy_mesh_cmd>();
        cmd->handle = handle;
        msh_model.set_render_built(false);
        msh_model.set_render_handle({});  // drop the soon-stale handle; rebuild reserves fresh
        ctx.rb->enqueue_cmd(cmd);
    }

    return result_code::ok;
}

/*===============================*/

result_code
texture__cmd_builder(reflection::type_context__render_cmd_build& ctx)
{
    auto& t = ctx.obj->asr<root::texture>();

    auto& bc = t.get_mutable_base_color();
    auto w = t.get_width();
    auto h = t.get_height();

    t.set_render_handle(ctx.rb->textures_alloc().reserve());

    auto* cmd = ctx.rb->alloc_cmd<create_texture_cmd>();
    cmd->id = t.get_id();
    cmd->handle = t.render_handle();
    cmd->width = w;
    cmd->height = h;

    if (::kryga::asset_importer::texture_importer::is_kryga_texture(bc.get_file()))
    {
        cmd->pixels = std::make_shared<utils::buffer>(bc);
        cmd->is_kryga_format = true;
    }
    else
    {
        auto pixels = std::make_shared<utils::buffer>();
        if (!kryga::asset_importer::texture_importer::extract_texture_from_buffer(
                bc, *pixels, w, h))
        {
            ALOG_LAZY_ERROR;
            return result_code::failed;
        }
        cmd->pixels = std::move(pixels);
        cmd->width = w;
        cmd->height = h;
        cmd->is_kryga_format = false;
    }

    t.set_render_built(true);
    ctx.rb->enqueue_cmd(cmd);

    return result_code::ok;
}

result_code
texture__cmd_destroyer(reflection::type_context__render_cmd_build& ctx)
{
    auto& txt_model = ctx.obj->asr<root::texture>();

    if (txt_model.get_render_built())
    {
        auto handle = txt_model.render_handle();
        ctx.rb->textures_alloc().free(handle);  // [model thread]

        auto* cmd = ctx.rb->alloc_cmd<destroy_texture_cmd>();
        cmd->handle = handle;
        txt_model.set_render_built(false);
        txt_model.set_render_handle({});  // rebuild reserves fresh
        ctx.rb->enqueue_cmd(cmd);
    }

    return result_code::ok;
}

/*===============================*/

result_code
sampler__cmd_builder(reflection::type_context__render_cmd_build& ctx)
{
    return result_code::ok;
}

result_code
sampler__cmd_destroyer(reflection::type_context__render_cmd_build& ctx)
{
    return result_code::ok;
}

/*===============================*/

result_code
shader_effect__cmd_builder(reflection::type_context__render_cmd_build& ctx)
{
    auto& se_model = ctx.obj->asr<root::shader_effect>();

    auto* cmd = ctx.rb->alloc_cmd<create_shader_effect_cmd>();
    cmd->id = se_model.get_id();
    cmd->vert = std::make_shared<utils::buffer>(se_model.m_vert);
    cmd->frag = std::make_shared<utils::buffer>(se_model.m_frag);
    cmd->is_vert_binary = se_model.m_is_vert_binary;
    cmd->is_frag_binary = se_model.m_is_frag_binary;
    cmd->wire_topology = se_model.m_wire_topology;
    cmd->enable_alpha = se_model.m_enable_alpha_support;
    cmd->spec_constants = render_convert::collect_spec_constants(se_model);

    se_model.set_render_built(true);
    ctx.rb->enqueue_cmd(cmd);

    return result_code::ok;
}

result_code
shader_effect__cmd_destroyer(reflection::type_context__render_cmd_build& ctx)
{
    auto& se_model = ctx.obj->asr<root::shader_effect>();

    if (se_model.get_render_built())
    {
        auto* cmd = ctx.rb->alloc_cmd<destroy_shader_effect_cmd>();
        cmd->id = se_model.get_id();
        se_model.set_render_built(false);
        ctx.rb->enqueue_cmd(cmd);
    }

    return result_code::ok;
}

/*===============================*/

result_code
material__cmd_builder(reflection::type_context__render_cmd_build& ctx)
{
    auto& mat_model = ctx.obj->asr<root::material>();

    auto collected = render_convert::collect_gpu_data(mat_model);

    std::vector<texture_slot_info> slots;
    for (uint32_t i = 0; i < collected.texture_slot_count; ++i)
    {
        auto& ts = *static_cast<const root::texture_slot*>(collected.texture_slots[i].data);

        texture_slot_info slot_info;
        slot_info.slot = collected.texture_slots[i].slot;

        if (ts.txt)
        {
            if (ctx.rb->render_cmd_build(*ts.txt, ctx.flag) != result_code::ok)
            {
                return result_code::failed;
            }
            slot_info.texture_handle = ts.txt->render_handle();
        }

        if (ts.smp)
        {
            ctx.rb->render_cmd_build(*ts.smp, ctx.flag);
            slot_info.static_sampler_index = render_convert::map_sampler_to_static_index(*ts.smp);
        }

        slots.push_back(slot_info);
    }

    auto se_model = mat_model.get_shader_effect();
    if (!se_model)
    {
        return result_code::failed;
    }
    ctx.rb->render_cmd_build(*se_model, ctx.flag);

    if (!mat_model.get_render_built())
    {
        if (!mat_model.render_handle())
        {
            mat_model.set_render_handle(ctx.rb->materials_alloc().reserve());
        }

        auto* cmd = ctx.rb->alloc_cmd<create_material_cmd>();
        cmd->id = mat_model.get_id();
        cmd->handle = mat_model.render_handle();
        cmd->type_id = mat_model.get_type_id();
        cmd->shader_effect_id = se_model->get_id();
        cmd->texture_slots = std::move(slots);
        cmd->gpu_data = std::move(collected.gpu_data);

        mat_model.set_render_built(true);
        ctx.rb->enqueue_cmd(cmd);
    }
    else
    {
        auto* cmd = ctx.rb->alloc_cmd<update_material_cmd>();
        cmd->handle = mat_model.render_handle();
        cmd->shader_effect_id = se_model->get_id();
        cmd->texture_slots = std::move(slots);
        cmd->gpu_data = std::move(collected.gpu_data);

        ctx.rb->enqueue_cmd(cmd);
    }

    return result_code::ok;
}

result_code
material__cmd_destroyer(reflection::type_context__render_cmd_build& ctx)
{
    auto& mat_model = ctx.obj->asr<root::material>();

    if (mat_model.get_render_built())
    {
        auto handle = mat_model.render_handle();
        ctx.rb->materials_alloc().free(handle);  // [model thread]

        auto* cmd = ctx.rb->alloc_cmd<destroy_material_cmd>();
        cmd->handle = handle;
        mat_model.set_render_built(false);
        mat_model.set_render_handle({});  // drop the soon-stale handle; rebuild reserves fresh
        ctx.rb->enqueue_cmd(cmd);
    }

    return result_code::ok;
}

/*===============================*/

result_code
game_object_component__cmd_builder(reflection::type_context__render_cmd_build& ctx)
{
    auto& t = ctx.obj->asr<root::game_object_component>();

    auto& c = t.get_render_children();

    for (auto child : c)
    {
        auto rc = ctx.rb->render_cmd_build(*child, false);
        KRG_return_nok(rc);
    }

    return result_code::ok;
}

result_code
game_object_component__cmd_destroyer(reflection::type_context__render_cmd_build& ctx)
{
    auto& t = ctx.obj->asr<root::game_object_component>();

    auto& c = t.get_render_children();

    for (auto child : c)
    {
        auto rc = ctx.rb->render_cmd_destroy(*child, false);
        KRG_return_nok(rc);
    }

    return result_code::ok;
}

/*===============================*/

}  // namespace root
}  // namespace kryga
