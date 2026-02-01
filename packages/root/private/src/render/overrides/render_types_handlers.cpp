#include "packages/root/package.root.h"

#include <render_bridge/render_bridge.h>

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
#include <core/global_state.h>
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

#include <utils/kryga_log.h>
#include <utils/string_utility.h>
#include <utils/dynamic_object_builder.h>

namespace kryga
{

namespace root
{

// Forward declaration
static uint8_t map_sampler_to_static_index(const sampler& smp);

result_code
mesh__render_loader(reflection::type_context__render& ctx)
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

    auto md = glob::vulkan_render_loader::get()->create_mesh(msh_model.get_id(), vertices, indices);

    msh_model.set_mesh_data(md);

    return result_code::ok;
}
result_code
mesh__render_destructor(reflection::type_context__render& ctx)
{
    auto& msh_model = ctx.obj->asr<root::mesh>();

    if (auto msh_data = msh_model.get_mesh_data())
    {
        glob::vulkan_render_loader::getr().destroy_mesh_data(msh_data->get_id());
    }

    return result_code::ok;
}

/*===============================*/

result_code
material__render_loader(reflection::type_context__render& ctx)
{
    auto& mat_model = ctx.obj->asr<root::material>();

    auto& txt_models = mat_model.get_texture_slots();

    // Collect texture samples and sampler indices
    std::vector<render::texture_sampler_data> samples_data;

    // Arrays for GPU material buffer (indexed by slot)
    uint32_t gpu_texture_indices[KGPU_MAX_TEXTURE_SLOTS];
    uint32_t gpu_sampler_indices[KGPU_MAX_TEXTURE_SLOTS];
    for (int i = 0; i < KGPU_MAX_TEXTURE_SLOTS; ++i)
    {
        gpu_texture_indices[i] = UINT32_MAX;  // Invalid index
        gpu_sampler_indices[i] = 0;           // Default to LINEAR_REPEAT
    }

    for (auto& ts : txt_models)
    {
        // Load texture
        if (ts.second.txt)
        {
            if (ctx.rb->render_ctor(*ts.second.txt, ctx.flag) != result_code::ok)
            {
                return result_code::failed;
            }
            samples_data.emplace_back();
            samples_data.back().texture = ts.second.txt->get_texture_data();
            samples_data.back().slot = ts.second.slot;

            // Store bindless index for GPU material buffer
            if (ts.second.slot < KGPU_MAX_TEXTURE_SLOTS)
            {
                gpu_texture_indices[ts.second.slot] =
                    ts.second.txt->get_texture_data()->get_bindless_index();
            }
        }

        // Load sampler and map to static index
        if (ts.second.smp)
        {
            ctx.rb->render_ctor(*ts.second.smp, ctx.flag);
            uint8_t smp_idx = map_sampler_to_static_index(*ts.second.smp);

            // Store sampler index for GPU material buffer
            if (ts.second.slot < KGPU_MAX_TEXTURE_SLOTS)
            {
                gpu_sampler_indices[ts.second.slot] = smp_idx;
            }
        }
    }

    auto se_model = mat_model.get_shader_effect();

    auto rc = ctx.rb->render_ctor(*se_model, ctx.flag);

    auto se_data = se_model->get_shader_effect_data();
    if (rc != result_code::ok || se_data->m_failed_load)
    {
        auto main_pass = glob::vulkan_render_loader::getr().get_render_pass(AID("main"));
        se_data = main_pass->get_shader_effect(AID("se_error"));
    }

    KRG_check(se_data, "Should exist");

    auto mat_data = glob::vulkan_render_loader::get()->get_material_data(mat_model.get_id());

    auto dyn_gpu_data = ctx.rb->collect_gpu_data(mat_model);

    // Set texture bindings in GPU data before creating/updating material
    render_bridge::set_material_texture_bindings(dyn_gpu_data, gpu_texture_indices,
                                                  gpu_sampler_indices, KGPU_MAX_TEXTURE_SLOTS);

    if (!mat_data)
    {
        mat_data = glob::vulkan_render_loader::get()->create_material(
            mat_model.get_id(), mat_model.get_type_id(), samples_data, *se_data, dyn_gpu_data);

        mat_model.set_material_data(mat_data);
    }
    else
    {
        glob::vulkan_render_loader::get()->update_material(*mat_data, samples_data, *se_data,
                                                           dyn_gpu_data);
    }

    // Set bindless sampler indices in material_data (for legacy code paths)
    for (auto& ts : txt_models)
    {
        if (ts.second.smp && ts.second.slot < KGPU_MAX_TEXTURE_SLOTS)
        {
            mat_data->set_bindless_sampler_index(ts.second.slot,
                                                  gpu_sampler_indices[ts.second.slot]);
        }
    }

    if (!dyn_gpu_data.empty())
    {
        glob::vulkan_render::getr().add_material(mat_data);
        glob::vulkan_render::getr().schedule_material_data_gpu_upload(mat_data);
    }

    return result_code::ok;
}
result_code
material__render_destructor(reflection::type_context__render& ctx)
{
    auto& mat_model = ctx.obj->asr<root::material>();

    if (auto mat_data = mat_model.get_material_data())
    {
        glob::vulkan_render::getr().drop_material(mat_data);
        glob::vulkan_render_loader::getr().destroy_material_data(mat_data->get_id());
    }

    return result_code::ok;
}

/*===============================*/

result_code
texture__render_loader(reflection::type_context__render& ctx)
{
    auto& t = ctx.obj->asr<root::texture>();

    auto& bc = t.get_mutable_base_color();
    auto w = t.get_width();
    auto h = t.get_height();

    render::texture_data* txt_data = nullptr;

    if (::kryga::render_bridge::is_kryga_texture(bc.get_file()))
    {
        txt_data = glob::vulkan_render_loader::get()->create_texture(t.get_id(), bc, w, h);
    }
    else
    {
        utils::buffer b;
        if (!kryga::asset_importer::texture_importer::extract_texture_from_buffer(bc, b, w, h))
        {
            ALOG_LAZY_ERROR;
            return result_code::failed;
        }
        txt_data = glob::vulkan_render_loader::get()->create_texture(t.get_id(), b, w, h);
    }
    t.set_texture_data(txt_data);

    return result_code::ok;
}
result_code
texture__render_destructor(reflection::type_context__render& ctx)
{
    auto& txt_model = ctx.obj->asr<root::texture>();

    if (auto txt_data = txt_model.get_texture_data())
    {
        glob::vulkan_render_loader::getr().destroy_texture_data(txt_data->id());
    }

    return result_code::ok;
}

/*===============================*/

result_code
game_object_component__render_loader(reflection::type_context__render& ctx)
{
    auto& t = ctx.obj->asr<root::game_object_component>();

    auto& c = t.get_children();

    for (auto& t : c)
    {
        auto rc = ctx.rb->render_ctor(*t, false);
        KRG_return_nok(rc);
    }

    return result_code::ok;
}
result_code
game_object_component__render_destructor(reflection::type_context__render& ctx)
{
    auto& t = ctx.obj->asr<root::game_object_component>();

    auto& c = t.get_children();

    for (auto& t : c)
    {
        auto rc = ctx.rb->render_dtor(*t, false);
        KRG_return_nok(rc);
    }

    return result_code::ok;
}

/*===============================*/

// Maps sampler model properties to static sampler index
static uint8_t
map_sampler_to_static_index(const sampler& smp)
{
    bool is_linear = (smp.get_min_filter() == sampler_filter::linear);
    auto addr = smp.get_address_u();  // Assume U and V are same for static samplers

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
    else  // nearest
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

    return KGPU_SAMPLER_LINEAR_REPEAT;  // Default fallback
}

result_code
sampler__render_loader(reflection::type_context__render& ctx)
{
    auto& smp_model = ctx.obj->asr<root::sampler>();

    // For now, samplers use static samplers - no render data needed
    // The mapping happens in material__render_loader when setting bindless indices
    // Could be extended later to create custom VkSamplers if needed

    return result_code::ok;
}

result_code
sampler__render_destructor(reflection::type_context__render& ctx)
{
    // Nothing to destroy - using static samplers
    return result_code::ok;
}

/*===============================*/

result_code
shader_effect__render_loader(reflection::type_context__render& ctx)
{
    auto& se_model = ctx.obj->asr<root::shader_effect>();

    auto se_ci = ::kryga::render_bridge::make_se_ci(se_model);

    auto rp = se_ci.rp;
    auto se_data = rp->get_shader_effect(se_model.get_id());

    if (!se_data)
    {
        auto rc = rp->create_shader_effect(se_model.get_id(), se_ci, se_data);

        KRG_check(se_data, "Should never happen!");

        se_model.set_shader_effect_data(se_data);

        return rc;
    }
    else
    {
        auto rc = rp->update_shader_effect(*se_data, se_ci);
        if (rc != result_code::ok)
        {
            ALOG_LAZY_ERROR;
            return rc;
        }
    }
    return result_code::ok;
}

result_code
shader_effect__render_destructor(reflection::type_context__render& ctx)
{
    auto& se_model = ctx.obj->asr<root::shader_effect>();

    if (auto se_data = se_model.get_shader_effect_data())
    {
        if (auto owner_rp = se_data->get_owner_render_pass())
        {
            owner_rp->destroy_shader_effect(se_data->get_id());
        }
    }

    return result_code::ok;
}

/*===============================*/

}  // namespace root
}  // namespace kryga
