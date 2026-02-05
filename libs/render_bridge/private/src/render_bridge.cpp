#include "render_bridge/render_bridge.h"

#include <core/reflection/reflection_type.h>

#include <packages/root/model/smart_object.h>
#include <packages/root/model/assets/shader_effect.h>
#include <packages/root/model/assets/material.h>
#include <glue/type_ids.ar.h>

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
#include <utils/dynamic_object_builder.h>

namespace kryga
{

glob::render_bridge::type glob::render_bridge::s_instance;

utils::dynobj
render_bridge::extract_gpu_data(root::smart_object& so, const access_template& ct)
{
    auto src_obj_ptr = so.as_blob();

    utils::dynobj dyn_obj(ct.layout);

    auto v = dyn_obj.root<render::gpu_type>();

    auto fn = v.field_count();

    // First 2 fields are texture_indices and sampler_indices arrays
    // Initialize them to invalid (UINT32_MAX) - they'll be set later via set_material_texture_bindings
    constexpr uint32_t INVALID_INDEX = UINT32_MAX;
    constexpr int num_binding_fields = 2;  // texture_indices array, sampler_indices array

    for (int i = 0; i < num_binding_fields; ++i)
    {
        auto field = v.field_by_idx(i);
        if (field)
        {
            // Fill entire array with invalid indices
            for (uint32_t j = 0; j < KGPU_MAX_TEXTURE_SLOTS; ++j)
            {
                memcpy(dyn_obj.data() + field->offset + j * sizeof(uint32_t), &INVALID_INDEX, sizeof(uint32_t));
            }
        }
    }

    // Skip binding fields, copy remaining material properties
    KRG_check(ct.offset_in_object.size() == fn - num_binding_fields, "Should match property count!");

    auto oitr = ct.offset_in_object.begin();
    uint64_t idx = num_binding_fields;  // Start after texture bindings
    while (auto field = v.field_by_idx(idx++))
    {
        memcpy(dyn_obj.data() + field->offset, src_obj_ptr + *oitr, field->size);
        ++oitr;
    }

    return dyn_obj;
}

void
render_bridge::set_material_texture_bindings(utils::dynobj& gpu_data,
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

bool
render_bridge::create_collection_template(root::smart_object& so, access_template& t)
{
    auto& properties = so.get_reflection()->m_properties;

    t.offset_in_object.clear();

    render::gpu_dynobj_builder sb;

    sb.set_id(AID("MaterialData"));

    // Texture bindings at the start of material GPU data (std430 layout)
    // std430: uint arrays have 4-byte stride (tightly packed)
    sb.add_array(AID("texture_indices"), render::gpu_type::g_unsigned, 4, KGPU_MAX_TEXTURE_SLOTS, 4);
    sb.add_array(AID("sampler_indices"), render::gpu_type::g_unsigned, 4, KGPU_MAX_TEXTURE_SLOTS, 4);

    for (auto& p : properties)
    {
        if (!p->gpu_data.empty())
        {
            switch (p->rtype->type_id)
            {
            case kryga::root__float:
                sb.add_field(AID(p->name), render::gpu_type::g_float, 1);
                break;
            case kryga::root__vec3:
                sb.add_field(AID(p->name), render::gpu_type::g_vec3, 16);
                break;
            case kryga::root__vec4:
                sb.add_field(AID(p->name), render::gpu_type::g_vec4, 16);
                break;
            default:
                KRG_never("Should never happen");
                break;
            }

            t.offset_in_object.push_back((uint32_t)p->offset);
        }
    }

    if (!sb.empty())
    {
        t.layout = sb.finalize();
    }

    return true;
}

utils::dynobj
render_bridge::collect_gpu_data(root::smart_object& so)
{
    auto* rt = so.get_reflection();

    if (!rt || !rt->gpu_pack || rt->gpu_data_size == 0)
    {
        return {};  // No GPU data for this type
    }

    // Create dynobj with raw buffer (no layout needed for compile-time structs)
    utils::dynobj result;
    result.resize(rt->gpu_data_size);

    // Pack model data into GPU struct
    rt->gpu_pack(&so, result.data());

    return result;
}

render::shader_effect_create_info
render_bridge::make_se_ci(root::shader_effect& se_model)
{
    render::shader_effect_create_info se_ci;
    se_ci.vert_buffer = &se_model.m_vert;
    se_ci.frag_buffer = &se_model.m_frag;
    se_ci.is_vert_binary = se_model.m_is_vert_binary;
    se_ci.is_frag_binary = se_model.m_is_frag_binary;
    se_ci.is_wire = se_model.m_wire_topology;
    se_ci.alpha =
        se_model.m_enable_alpha_support ? render::alpha_mode::world : render::alpha_mode::none;
    se_ci.rp = glob::vulkan_render::getr().get_render_pass(AID("main"));
    se_ci.enable_dynamic_state = false;
    se_ci.ds_mode = render::depth_stencil_mode::none;

    se_ci.cull_mode = se_ci.is_wire ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;

    return se_ci;
}

std::string
render_bridge::make_qid(render::material_data& mt_data, render::mesh_data& m_data)
{
    if (mt_data.get_shader_effect()->m_enable_alpha)
    {
        return "transparent";
    }

    return mt_data.get_id().str() + "::" + m_data.get_id().str();
}

bool
render_bridge::is_kryga_texture(const utils::path& p)
{
    return p.has_extension(".atbc");
}

bool
render_bridge::is_kryga_mesh(const utils::path& p)
{
    return p.has_extension(".avrt") || p.has_extension(".aind");
}

kryga::result_code
render_bridge::render_ctor(root::smart_object& obj, bool sub_objects)
{
    KRG_check(obj.get_flags().instance_obj, "");

    if (obj.get_state() == root::smart_object_state::render_ready)
    {
        return result_code::ok;
    }

    // KRG_check(obj.get_state() == root::smart_object_state::constructed, "Should not happen");

    obj.set_state(root::smart_object_state::render_preparing);

    get_dependency().build_node(&obj);

    reflection::type_context__render render_ctx{this, &obj, sub_objects};
    result_code rc = obj.get_reflection()->render_constructor(render_ctx);

    obj.set_state(root::smart_object_state::render_ready);

    return rc;
}

kryga::result_code
render_bridge::render_dtor(root::smart_object& obj, bool sub_objects)
{
    KRG_check(obj.get_flags().instance_obj, "");

    if (obj.get_state() == root::smart_object_state::constructed)
    {
        return result_code::ok;
    }

    KRG_check(obj.get_state() == root::smart_object_state::render_ready, "Should not happen");

    obj.set_state(root::smart_object_state::render_preparing);

    reflection::type_context__render render_ctx{this, &obj, sub_objects};
    result_code rc = obj.get_reflection()->render_destructor(render_ctx);

    obj.set_state(root::smart_object_state::constructed);

    return rc;
}

}  // namespace kryga