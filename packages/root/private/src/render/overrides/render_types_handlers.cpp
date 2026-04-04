#include "packages/root/package.root.h"

#include <global_state/global_state.h>
#include <render_bridge/render_bridge.h>
#include <render_bridge/render_command.h>

#include "packages/root/model/assets/mesh.h"
#include "packages/root/model/assets/material.h"
#include "packages/root/model/assets/texture.h"
#include "packages/root/model/assets/sampler.h"
#include "packages/root/model/assets/shader_effect.h"

#include <gpu_types/gpu_generic_constants.h>
#include <glue/type_ids.ar.h>

#include "packages/root/model/game_object.h"

#include <core/reflection/reflection_type.h>
#include <core/reflection/property_utils.h>
#include <core/caches/cache_set.h>
#include <core/object_load_context.h>
#include <core/object_constructor.h>
#include <core/package.h>
#include <core/reflection/reflection_type_utils.h>

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

// Forward declaration
static uint8_t
map_sampler_to_static_index(const sampler& smp);

// Maps sampler model properties to static sampler index
static uint8_t
map_sampler_to_static_index(const sampler& smp)
{
    bool is_linear = (smp.get_min_filter() == sampler_filter::linear);
    auto addr = smp.get_address_u();

    if (smp.get_anisotropy() && is_linear && addr == sampler_address::repeat)
    {
        return KGPU_SAMPLER_ANISO_REPEAT;
    }

    if (is_linear)
    {
        switch (addr)
        {
        case sampler_address::repeat:
            return KGPU_SAMPLER_LINEAR_REPEAT;
        case sampler_address::mirrored_repeat:
            return KGPU_SAMPLER_LINEAR_MIRROR;
        case sampler_address::clamp_to_edge:
            return KGPU_SAMPLER_LINEAR_CLAMP;
        case sampler_address::clamp_to_border:
            return KGPU_SAMPLER_LINEAR_CLAMP_BORDER;
        }
    }
    else
    {
        switch (addr)
        {
        case sampler_address::repeat:
        case sampler_address::mirrored_repeat:
            return KGPU_SAMPLER_NEAREST_REPEAT;
        case sampler_address::clamp_to_edge:
        case sampler_address::clamp_to_border:
            return KGPU_SAMPLER_NEAREST_CLAMP;
        }
    }

    return KGPU_SAMPLER_LINEAR_REPEAT;
}

// ============================================================================
// Render commands
// ============================================================================

struct create_mesh_cmd : render_cmd::render_command_base
{
    utils::id id;
    std::shared_ptr<utils::buffer> vertices;
    std::shared_ptr<utils::buffer> indices;
    bool skinned = false;

    void
    execute(render_cmd::render_exec_context& ctx) override
    {
        // Skip if mesh already exists (e.g. plane_mesh created by system init
        // and also referenced by billboard debug components)
        if (ctx.loader.get_mesh_data(id))
        {
            return;
        }

        if (skinned)
        {
            auto vbv = vertices->make_view<gpu::skinned_vertex_data>();
            auto ibv = indices->make_view<gpu::uint>();
            ctx.loader.create_skinned_mesh(id, vbv, ibv);
        }
        else
        {
            auto vbv = vertices->make_view<gpu::vertex_data>();
            auto ibv = indices->make_view<gpu::uint>();
            ctx.loader.create_mesh(id, vbv, ibv);
        }
    }
};

struct destroy_mesh_cmd : render_cmd::render_command_base
{
    utils::id id;

    void
    execute(render_cmd::render_exec_context& ctx) override
    {
        ctx.loader.destroy_mesh_data(id);
    }
};

struct create_texture_cmd : render_cmd::render_command_base
{
    utils::id id;
    std::shared_ptr<utils::buffer> pixels;
    uint32_t width = 0;
    uint32_t height = 0;
    bool is_kryga_format = false;

    void
    execute(render_cmd::render_exec_context& ctx) override
    {
        ctx.loader.create_texture(id, *pixels, width, height);
    }
};

struct destroy_texture_cmd : render_cmd::render_command_base
{
    utils::id id;

    void
    execute(render_cmd::render_exec_context& ctx) override
    {
        ctx.loader.destroy_texture_data(id);
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
    utils::id texture_id;
    uint8_t static_sampler_index = 0;
};

struct create_material_cmd : render_cmd::render_command_base
{
    utils::id id;
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
            if (slot.texture_id.valid())
            {
                auto* td = ctx.loader.get_texture_data(slot.texture_id);
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
            if (slot.texture_id.valid() && slot.slot < KGPU_MAX_TEXTURE_SLOTS)
            {
                auto* td = ctx.loader.get_texture_data(slot.texture_id);
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

        render_bridge::set_material_texture_bindings(
            gpu_data, gpu_texture_indices, gpu_sampler_indices, KGPU_MAX_TEXTURE_SLOTS);

        auto* mat_data = ctx.loader.create_material(id, type_id, samples, *se_data, gpu_data);

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
                ctx.vr.schd_add_material(mat_data);
            }
        }
    }
};

struct update_material_cmd : render_cmd::render_command_base
{
    utils::id id;
    utils::id shader_effect_id;
    std::vector<texture_slot_info> texture_slots;
    utils::dynobj gpu_data;

    void
    execute(render_cmd::render_exec_context& ctx) override
    {
        auto* mat_data = ctx.loader.get_material_data(id);
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
            if (slot.texture_id.valid())
            {
                auto* td = ctx.loader.get_texture_data(slot.texture_id);
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
            if (slot.texture_id.valid() && slot.slot < KGPU_MAX_TEXTURE_SLOTS)
            {
                auto* td = ctx.loader.get_texture_data(slot.texture_id);
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

        render_bridge::set_material_texture_bindings(
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
            ctx.vr.schd_update_material(mat_data);
        }
    }
};

struct destroy_material_cmd : render_cmd::render_command_base
{
    utils::id id;

    void
    execute(render_cmd::render_exec_context& ctx) override
    {
        auto* mat_data = ctx.loader.get_material_data(id);
        if (mat_data)
        {
            ctx.vr.schd_remove_material(mat_data);
            ctx.loader.destroy_material_data(id);
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
    float max_dist_sq = 0.0f;
    for (size_t i = 0; i < vbv.size(); ++i)
    {
        const auto& pos = vbv.at(i).position;
        float dist_sq = glm::dot(pos, pos);
        max_dist_sq = std::max(max_dist_sq, dist_sq);
    }
    msh_model.set_bounding_radius(std::sqrt(max_dist_sq));

    auto* cmd = ctx.rb->alloc_cmd<create_mesh_cmd>();
    cmd->id = msh_model.get_id();
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
        auto* cmd = ctx.rb->alloc_cmd<destroy_mesh_cmd>();
        cmd->id = msh_model.get_id();
        msh_model.set_render_built(false);
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

    auto* cmd = ctx.rb->alloc_cmd<create_texture_cmd>();
    cmd->id = t.get_id();
    cmd->width = w;
    cmd->height = h;

    if (::kryga::render_bridge::is_kryga_texture(bc.get_file()))
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
        auto* cmd = ctx.rb->alloc_cmd<destroy_texture_cmd>();
        cmd->id = txt_model.get_id();
        txt_model.set_render_built(false);
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

    auto& txt_models = mat_model.get_texture_slots();

    std::vector<texture_slot_info> slots;

    for (auto& ts : txt_models)
    {
        texture_slot_info slot_info;
        slot_info.slot = ts.second.slot;

        if (ts.second.txt)
        {
            if (ctx.rb->render_cmd_build(*ts.second.txt, ctx.flag) != result_code::ok)
            {
                return result_code::failed;
            }
            slot_info.texture_id = ts.second.txt->get_id();
        }

        if (ts.second.smp)
        {
            ctx.rb->render_cmd_build(*ts.second.smp, ctx.flag);
            slot_info.static_sampler_index = map_sampler_to_static_index(*ts.second.smp);
        }

        slots.push_back(slot_info);
    }

    auto se_model = mat_model.get_shader_effect();
    ctx.rb->render_cmd_build(*se_model, ctx.flag);

    auto dyn_gpu_data = ctx.rb->collect_gpu_data(mat_model);

    if (!mat_model.get_render_built())
    {
        auto* cmd = ctx.rb->alloc_cmd<create_material_cmd>();
        cmd->id = mat_model.get_id();
        cmd->type_id = mat_model.get_type_id();
        cmd->shader_effect_id = se_model->get_id();
        cmd->texture_slots = std::move(slots);
        cmd->gpu_data = std::move(dyn_gpu_data);

        mat_model.set_render_built(true);
        ctx.rb->enqueue_cmd(cmd);
    }
    else
    {
        auto* cmd = ctx.rb->alloc_cmd<update_material_cmd>();
        cmd->id = mat_model.get_id();
        cmd->shader_effect_id = se_model->get_id();
        cmd->texture_slots = std::move(slots);
        cmd->gpu_data = std::move(dyn_gpu_data);

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
        auto* cmd = ctx.rb->alloc_cmd<destroy_material_cmd>();
        cmd->id = mat_model.get_id();
        mat_model.set_render_built(false);
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
