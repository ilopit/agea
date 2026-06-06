#include "render_bridge/render_translate.h"

#include <global_state/global_state.h>
#include <core/reflection/reflection_type.h>
#include <core/lightmap_manifest.h>

#include <packages/root/model/smart_object.h>
#include <packages/root/model/assets/shader_effect.h>
#include <packages/root/model/assets/material.h>
#include <packages/root/model/assets/sampler.h>
#include <gpu_types/gpu_generic_constants.h>

#include <vulkan_render/types/vulkan_material_data.h>
#include <vulkan_render/types/vulkan_mesh_data.h>
#include <vulkan_render/types/vulkan_shader_effect_data.h>
#include <vulkan_render/kryga_render.h>
#include <vulkan_render/render_system.h>

namespace kryga
{
namespace render_translate
{

void
set_material_texture_bindings(utils::dynobj& gpu_data,
                              const uint32_t* texture_indices,
                              const uint32_t* sampler_indices,
                              uint32_t slot_count)
{
    if (gpu_data.empty())
    {
        return;
    }

    // GPU struct layout: texture_indices[KGPU_MAX_TEXTURE_SLOTS] at offset 0,
    // sampler_indices[KGPU_MAX_TEXTURE_SLOTS] immediately after
    constexpr size_t tex_offset = 0;
    constexpr size_t smp_offset = KGPU_MAX_TEXTURE_SLOTS * sizeof(uint32_t);

    uint32_t count = std::min(slot_count, (uint32_t)KGPU_MAX_TEXTURE_SLOTS);

    memcpy(gpu_data.data() + tex_offset, texture_indices, count * sizeof(uint32_t));
    memcpy(gpu_data.data() + smp_offset, sampler_indices, count * sizeof(uint32_t));
}

std::unordered_map<std::string, uint32_t>
collect_spec_constants(root::smart_object& so)
{
    std::unordered_map<std::string, uint32_t> result;

    auto* rt = so.get_reflection();
    if (!rt)
    {
        return result;
    }

    auto ptr = so.as_blob();

    for (const auto& prop : rt->m_properties)
    {
        if (prop->category != "Specialization")
        {
            continue;
        }

        // Read the bool value at the property offset
        bool value = *reinterpret_cast<const bool*>(ptr + prop->offset);
        if (value)
        {
            // Convert property name to shader constant name:
            // m_enable_lightmap → ENABLE_LIGHTMAP
            std::string name = prop->name;
            for (auto& c : name)
            {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
            result[name] = 1;
        }
    }

    return result;
}

collected_gpu_data
collect_gpu_data(root::smart_object& so)
{
    auto* rt = so.get_reflection();

    collected_gpu_data result;

    if (rt && rt->gpu_pack && rt->gpu_data_size > 0)
    {
        result.gpu_data.resize(rt->gpu_data_size);
        rt->gpu_pack(&so, result.gpu_data.data());
    }

    if (rt && rt->gpu_texture_collect)
    {
        result.texture_slot_count = rt->gpu_texture_collect(&so, result.texture_slots);
    }

    return result;
}

render::shader_effect_create_info
make_se_ci(root::shader_effect& se_model)
{
    render::shader_effect_create_info se_ci;
    se_ci.vert_buffer = &se_model.m_vert;
    se_ci.frag_buffer = &se_model.m_frag;
    se_ci.is_vert_binary = se_model.m_is_vert_binary;
    se_ci.is_frag_binary = se_model.m_is_frag_binary;
    se_ci.is_wire = se_model.m_wire_topology;
    se_ci.alpha =
        se_model.m_enable_alpha_support ? render::alpha_mode::world : render::alpha_mode::none;
    se_ci.rp = glob::glob_state().getr_render().renderer.get_render_pass(AID("main"));
    se_ci.enable_dynamic_state = false;
    se_ci.ds_mode = render::depth_stencil_mode::none;

    se_ci.cull_mode = se_ci.is_wire ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;

    return se_ci;
}

std::string
make_qid(render::material_data& mt_data, render::mesh_data& m_data)
{
    if (mt_data.get_shader_effect()->m_enable_alpha)
    {
        return "transparent";
    }

    return mt_data.get_id().str() + "::" + m_data.get_id().str();
}

std::string
make_qid_from_model(root::smart_object& mat_obj, root::smart_object& mesh_obj)
{
    auto& mat_model = mat_obj.asr<root::material>();
    auto* se = mat_model.get_shader_effect();

    if (se && se->m_enable_alpha_support)
    {
        return "transparent";
    }

    return mat_model.get_id().str() + "::" + mesh_obj.get_id().str();
}

uint8_t
map_sampler_to_static_index(const root::sampler& smp)
{
    bool is_linear = (smp.get_min_filter() == root::sampler_filter::linear);
    auto addr = smp.get_address_u();

    if (smp.get_anisotropy() && is_linear && addr == root::sampler_address::repeat)
    {
        return KGPU_SAMPLER_ANISO_REPEAT;
    }

    if (is_linear)
    {
        switch (addr)
        {
        case root::sampler_address::repeat:
            return KGPU_SAMPLER_LINEAR_REPEAT;
        case root::sampler_address::mirrored_repeat:
            return KGPU_SAMPLER_LINEAR_MIRROR;
        case root::sampler_address::clamp_to_edge:
            return KGPU_SAMPLER_LINEAR_CLAMP;
        case root::sampler_address::clamp_to_border:
            return KGPU_SAMPLER_LINEAR_CLAMP_BORDER;
        }
    }
    else
    {
        switch (addr)
        {
        case root::sampler_address::repeat:
        case root::sampler_address::mirrored_repeat:
            return KGPU_SAMPLER_NEAREST_REPEAT;
        case root::sampler_address::clamp_to_edge:
        case root::sampler_address::clamp_to_border:
            return KGPU_SAMPLER_NEAREST_CLAMP;
        }
    }

    return KGPU_SAMPLER_LINEAR_REPEAT;
}

std::unordered_map<utils::id, render::lightmap_uv>
flatten_lightmap_manifest(const core::lightmap_manifest& manifest)
{
    std::unordered_map<utils::id, render::lightmap_uv> out;
    out.reserve(manifest.objects.size());
    for (const auto& [oid, entry] : manifest.objects)
    {
        out[oid] = render::lightmap_uv{entry.lightmap_scale, entry.lightmap_offset};
    }
    return out;
}

}  // namespace render_translate
}  // namespace kryga
