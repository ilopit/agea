#include "render_bridge/render_bridge.h"
#include "render_bridge/render_commands_common.h"

#include <global_state/global_state.h>
#include <core/queues.h>
#include <core/reflection/reflection_type.h>

#include <packages/root/model/smart_object.h>
#include <packages/root/model/components/game_object_component.h>
#include <packages/root/model/game_object.h>
#include <packages/root/model/assets/shader_effect.h>
#include <packages/root/model/assets/material.h>
#include <packages/root/model/assets/sampler.h>
#include <glue/type_ids.ar.h>
#include <gpu_types/gpu_generic_constants.h>

#include <vulkan_render/utils/vulkan_initializers.h>
#include <vulkan_render/types/vulkan_render_data.h>
#include <vulkan_render/types/vulkan_mesh_data.h>
#include <vulkan_render/types/vulkan_texture_data.h>
#include <vulkan_render/types/vulkan_material_data.h>
#include <vulkan_render/types/vulkan_gpu_types.h>
#include <vulkan_render/types/vulkan_shader_effect_data.h>
#include <vulkan_render/types/vulkan_shader_data.h>
#include <vulkan_render/vulkan_render_loader.h>
#include <vulkan_render/vulkan_render_device.h>
#include <vulkan_render/kryga_render.h>
#include <vulkan_render/render_system.h>

#include <utils/kryga_log.h>
#include <utils/dynamic_object_builder.h>

namespace kryga
{

void
state_mutator__render_bridge::set(gs::state& s)
{
    auto p = s.create_box<render_bridge>("render_bridge");
    s.m_render_bridge = p;
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

std::unordered_map<std::string, uint32_t>
render_bridge::collect_spec_constants(root::smart_object& so)
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
render_bridge::collect_gpu_data(root::smart_object& so)
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
    se_ci.rp = glob::glob_state().getr_render().renderer.get_render_pass(AID("main"));
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

std::string
render_bridge::make_qid_from_model(root::smart_object& mat_obj, root::smart_object& mesh_obj)
{
    auto& mat_model = mat_obj.asr<root::material>();
    auto* se = mat_model.get_shader_effect();

    if (se && se->m_enable_alpha_support)
    {
        return "transparent";
    }

    return mat_model.get_id().str() + "::" + mesh_obj.get_id().str();
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

void*
render_bridge::alloc_cmd_raw(size_t size, size_t align)
{
    return glob::glob_state().getr_queues().get_render().alloc_raw(size, align);
}

void
render_bridge::enqueue_cmd(render_cmd::render_command_base* cmd)
{
    glob::glob_state().getr_queues().get_render().enqueue(cmd);
}

void
render_bridge::drain_frame(uint32_t slot)
{
    auto& vr = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;

    render_cmd::render_exec_context exec_ctx{vr, loader};

    // Drain this slot's queue to empty. All of the frame's commands were pushed
    // (and made visible via the submitted-counter mutex handoff) before the
    // render thread was released, and the producer is on the other slot, so
    // "empty" reliably means "whole frame consumed".
    glob::glob_state().getr_queues().get_render().command_queue(slot).drain(
        [&exec_ctx](render_cmd::render_command_base*&& cmd)
        {
            cmd->execute(exec_ctx);
            cmd->~render_command_base();
        });
}

void
render_bridge::set_active_slot(uint32_t slot)
{
    glob::glob_state().getr_queues().get_render().set_active_slot(slot);
}

void
render_bridge::reset_slot(uint32_t slot)
{
    glob::glob_state().getr_queues().get_render().reset_slot(slot);
}

void
render_bridge::reset_arena()
{
    glob::glob_state().getr_queues().get_render().reset_arena();
}

kryga::result_code
render_bridge::render_cmd_build(root::smart_object& obj, bool sub_objects)
{
    KRG_check(!obj.get_flags().default_obj, "CDOs must not be render-built");

    if (obj.get_state() == root::smart_object_state::render_ready)
    {
        return result_code::ok;
    }

    auto build_fn = obj.get_reflection()->render_cmd_builder;
    if (!build_fn)
    {
        obj.set_state(root::smart_object_state::render_ready);
        return result_code::ok;
    }

    obj.set_state(root::smart_object_state::render_preparing);

    get_dependency().build_node(&obj);

    reflection::type_context__render_cmd_build ctx{this, &obj, sub_objects};
    result_code rc = build_fn(ctx);

    obj.set_state(root::smart_object_state::render_ready);

    return rc;
}

kryga::result_code
render_bridge::render_cmd_destroy(root::smart_object& obj, bool sub_objects)
{
    // Class-default objects (CDOs) are shared, readonly templates referenced by
    // instances — never render-built. A type's CDO can be loaded on first use
    // mid-play and swept into a level rollback, and a runtime component's
    // recursive destroy can also reach a CDO it merely references. Skip rather
    // than assert: there are no render resources to free, and tearing into shared
    // state would corrupt surviving objects. (Package-owned shared assets are
    // kept out of the recursion by is_same_source in the per-type destroyers.)
    if (obj.get_flags().default_obj)
    {
        return result_code::ok;
    }

    if (obj.get_state() == root::smart_object_state::constructed)
    {
        return result_code::ok;
    }

    KRG_check(obj.get_state() == root::smart_object_state::render_ready, "Should not happen");

    auto destroy_fn = obj.get_reflection()->render_cmd_destroyer;
    if (!destroy_fn)
    {
        obj.set_state(root::smart_object_state::constructed);
        return result_code::ok;
    }

    obj.set_state(root::smart_object_state::render_preparing);

    reflection::type_context__render_cmd_build ctx{this, &obj, sub_objects};
    result_code rc = destroy_fn(ctx);

    obj.set_state(root::smart_object_state::constructed);

    return rc;
}

kryga::result_code
render_bridge::render_cmd_transform(root::game_object_component& source)
{
    auto r = source.get_owner()->get_components(source.get_order_idx());

    for (auto& obj : r)
    {
        auto handler = obj.get_reflection()->render_cmd_transform;
        if (!handler)
        {
            continue;
        }

        reflection::type_context__render_cmd_build ctx{this, &obj};
        handler(ctx);
    }

    return result_code::ok;
}

uint8_t
render_bridge::map_sampler_to_static_index(const root::sampler& smp)
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

// ============================================================================
// Common commands
// ============================================================================

void
update_transform_cmd::execute(render_cmd::render_exec_context& ctx)
{
    auto* object_data = ctx.vr.get_cache().objects.find_by_id(id);
    if (!object_data)
    {
        return;
    }

    object_data->gpu_data.model = transform;
    object_data->gpu_data.normal = normal_matrix;
    object_data->gpu_data.obj_pos = position;
    object_data->gpu_data.bounding_sphere_center = bounding_sphere_center;
    object_data->gpu_data.bounding_radius = bounding_radius;

    ctx.vr.schd_update_object(object_data);
}

void
set_outline_cmd::execute(render_cmd::render_exec_context& ctx)
{
    auto* object_data = ctx.vr.get_cache().objects.find_by_id(id);
    if (!object_data)
    {
        // Object already destroyed (e.g. selection cleared during level unload).
        return;
    }

    object_data->outlined = outlined;
    ctx.vr.schd_update_object_queue(object_data);
}

}  // namespace kryga
